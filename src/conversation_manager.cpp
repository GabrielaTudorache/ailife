#include "conversation_manager.h"

#include "paths.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <system_error>
#include <utility>

namespace {
constexpr int kCleanupThresholdSec = 30;
} // namespace

ConversationManager::ConversationManager(int my_pid, std::string my_name,
                                         std::chrono::system_clock::time_point birth_at, Logger& logger)
    : my_pid_(my_pid), my_name_(std::move(my_name)), birth_at_(birth_at), logger_(&logger) {}

long long ConversationManager::birthMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(birth_at_.time_since_epoch()).count();
}

int ConversationManager::parsePartnerPidFromDir(const std::string& dir_name) const {
    // dir_name is "<pid1>_<pid2>", return the other pid
    const auto underscore = dir_name.find('_');
    if (underscore == std::string::npos) {
        return 0;
    }
    int pid1 = 0;
    int pid2 = 0;
    auto parse = [](const std::string& s, int& out) {
        const auto* first = s.data();
        const auto* last = s.data() + s.size();
        auto [ptr, ec] = std::from_chars(first, last, out);
        return ec == std::errc{} && ptr == last;
    };
    if (!parse(dir_name.substr(0, underscore), pid1)) {
        return 0;
    }
    if (!parse(dir_name.substr(underscore + 1), pid2)) {
        return 0;
    }
    if (pid1 != my_pid_ && pid2 != my_pid_) {
        return 0;
    }
    return pid1 == my_pid_ ? pid2 : pid1;
}

std::vector<InboxUpdate> ConversationManager::pollInbox(const std::vector<PresenceSnapshot>& presences) {
    std::vector<InboxUpdate> updates;
    std::error_code ec;
    const auto root = Paths::conversationsDirectory();
    const long long birth_ms = birthMs();

    std::optional<InboxUpdate> active_update;
    std::optional<InboxUpdate> opener_update;
    long long opener_freshness = 0;

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory()) {
            continue;
        }
        const auto dir_name = entry.path().filename().string();
        const int partner_pid = parsePartnerPidFromDir(dir_name);
        if (partner_pid == 0) {
            continue;
        }

        const auto watermark_it = last_seen_ms_per_channel_.find(partner_pid);
        long long since = watermark_it == last_seen_ms_per_channel_.end() ? birth_ms : watermark_it->second + 1;

        ConversationChannel channel{my_pid_, partner_pid};
        auto messages = channel.read(since, *logger_);
        if (messages.empty()) {
            continue;
        }

        long long max_ts = since - 1;
        for (const auto& m : messages) {
            max_ts = std::max(max_ts, timestampMs(m.timestamp));
        }
        last_seen_ms_per_channel_[partner_pid] = max_ts;

        for (auto& m : messages) {
            if (m.from_pid == my_pid_) {
                continue;
            }
            if (m.is_end) {
                if (active_partner_pid_ == partner_pid) {
                    clearActive();
                }
                continue;
            }
            std::string partner_name = m.from_name;
            for (const auto& p : presences) {
                if (p.pid == partner_pid) {
                    partner_name = p.name;
                    break;
                }
            }

            if (active_partner_pid_ != 0 && partner_pid == active_partner_pid_) {
                if (!active_update.has_value()) {
                    InboxUpdate u;
                    u.partner_pid = partner_pid;
                    u.partner_name = partner_name;
                    u.message = m;
                    u.is_active_partner = true;
                    u.is_new_conversation = false;
                    active_update = std::move(u);
                    last_incoming_at_ = m.timestamp;
                    active_messages_.push_back(m);
                }
            } else if (active_partner_pid_ == 0) {
                const long long ts = timestampMs(m.timestamp);
                if (!opener_update.has_value() || ts > opener_freshness) {
                    InboxUpdate u;
                    u.partner_pid = partner_pid;
                    u.partner_name = partner_name;
                    u.message = m;
                    u.is_active_partner = false;
                    u.is_new_conversation = true;
                    opener_update = std::move(u);
                    opener_freshness = ts;
                }
            } else {
                InboxUpdate u;
                u.partner_pid = partner_pid;
                u.partner_name = partner_name;
                u.message = m;
                u.is_active_partner = false;
                u.is_new_conversation = false;
                updates.push_back(std::move(u));
            }
        }
    }

    if (active_update.has_value()) {
        updates.push_back(std::move(*active_update));
    } else if (opener_update.has_value()) {
        updates.push_back(std::move(*opener_update));
    }
    return updates;
}

bool ConversationManager::initiate(int partner_pid, std::string partner_name, const std::string& opener, Tone tone) {
    if (active_partner_pid_ != 0) {
        return false;
    }
    active_partner_pid_ = partner_pid;
    active_partner_name_ = std::move(partner_name);
    active_started_at_ = std::chrono::system_clock::now();
    last_incoming_at_ = active_started_at_;
    next_sequence_ = 1;
    active_messages_.clear();
    active_channel_ = std::make_unique<ConversationChannel>(my_pid_, partner_pid);

    Message m;
    m.from_pid = my_pid_;
    m.to_pid = partner_pid;
    m.from_name = my_name_;
    m.to_name = active_partner_name_;
    m.content = opener;
    m.tone = tone;
    m.sequence = next_sequence_++;
    m.timestamp = std::chrono::system_clock::now();
    const long long ts = active_channel_->append(m);
    m.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{ts}};
    active_messages_.push_back(m);
    return true;
}

void ConversationManager::reply(const std::string& text, Tone tone) {
    if (active_partner_pid_ == 0 || active_channel_ == nullptr) {
        return;
    }
    Message m;
    m.from_pid = my_pid_;
    m.to_pid = active_partner_pid_;
    m.from_name = my_name_;
    m.to_name = active_partner_name_;
    m.content = text;
    m.tone = tone;
    m.sequence = next_sequence_++;
    m.timestamp = std::chrono::system_clock::now();
    const long long ts = active_channel_->append(m);
    m.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{ts}};
    active_messages_.push_back(m);
}

void ConversationManager::closeActive(EndReason reason, const std::string& closing_text) {
    if (active_partner_pid_ == 0 || active_channel_ == nullptr) {
        return;
    }
    Message m;
    m.is_end = true;
    m.from_pid = my_pid_;
    m.to_pid = active_partner_pid_;
    m.from_name = my_name_;
    m.to_name = active_partner_name_;
    m.content = closing_text;
    m.tone = Tone::Neutral;
    m.sequence = next_sequence_++;
    m.timestamp = std::chrono::system_clock::now();
    m.end_reason = reason;
    active_channel_->append(m);
    clearActive();
}

std::optional<EndReason> ConversationManager::handleTimeouts(const std::vector<PresenceSnapshot>& presences,
                                                             std::chrono::seconds timeout) {
    if (active_partner_pid_ == 0) {
        return std::nullopt;
    }

    bool partner_present = false;
    bool partner_dead = false;
    for (const auto& p : presences) {
        if (p.pid == active_partner_pid_) {
            partner_present = true;
            const auto age =
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - p.last_heartbeat);
            if (age > std::chrono::seconds{kCleanupThresholdSec}) {
                partner_dead = true;
            }
            break;
        }
    }
    if (!partner_present || partner_dead) {
        closeActive(EndReason::PartnerGone, "");
        return EndReason::PartnerGone;
    }

    const auto since_incoming =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last_incoming_at_);
    if (since_incoming > timeout && active_messages_.size() >= 1) {
        bool waiting_on_partner = false;
        if (!active_messages_.empty()) {
            const auto& last = active_messages_.back();
            waiting_on_partner = (last.from_pid == my_pid_);
        }
        if (waiting_on_partner) {
            closeActive(EndReason::Timeout, "");
            return EndReason::Timeout;
        }
    }

    return std::nullopt;
}

bool ConversationManager::hasActiveConversation() const {
    return active_partner_pid_ != 0;
}

std::optional<int> ConversationManager::activePartnerPid() const {
    if (active_partner_pid_ == 0) {
        return std::nullopt;
    }
    return active_partner_pid_;
}

std::optional<std::string> ConversationManager::activePartnerName() const {
    if (active_partner_pid_ == 0) {
        return std::nullopt;
    }
    return active_partner_name_;
}

int ConversationManager::activeMessageCount() const {
    return static_cast<int>(active_messages_.size());
}

const std::deque<Message>& ConversationManager::activeTranscript() const {
    return active_messages_;
}

std::chrono::system_clock::time_point ConversationManager::lastIncomingAt() const {
    return last_incoming_at_;
}

void ConversationManager::clearActive() {
    active_partner_pid_ = 0;
    active_partner_name_.clear();
    active_started_at_ = {};
    last_incoming_at_ = {};
    next_sequence_ = 1;
    active_messages_.clear();
    active_channel_.reset();
}
