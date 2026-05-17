#include "spawn_process.h"

#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace {
std::string executablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH]{};
    const DWORD size = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (size == 0 || size == MAX_PATH) {
        throw std::runtime_error("could not determine executable path");
    }
    return std::string{buffer, size};
#else
    char buffer[PATH_MAX]{};
    const ssize_t size = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (size <= 0) {
        throw std::runtime_error("could not determine executable path");
    }
    return std::string{buffer, static_cast<std::size_t>(size)};
#endif
}

std::vector<std::string> buildArgs(const std::string& exe, const SpawnOptions& options, bool mock_llm) {
    std::vector<std::string> args{
        exe,           "--spawn",         "--name",     options.name,
        "--archetype", options.archetype, "--duration", std::to_string(options.lifespan_minutes),
        "--headless"};
    if (mock_llm) {
        args.emplace_back("--mock-llm");
    }
    if (!options.appearance.hat.empty()) {
        args.emplace_back("--hat");
        args.push_back(options.appearance.hat);
    }
    if (!options.appearance.eyes.empty()) {
        args.emplace_back("--eyes");
        args.push_back(options.appearance.eyes);
    }
    if (!options.appearance.mouth.empty()) {
        args.emplace_back("--mouth");
        args.push_back(options.appearance.mouth);
    }
    if (options.custom_personality) {
        args.emplace_back("--openness");
        args.push_back(std::to_string(options.personality.openness));
        args.emplace_back("--conscientiousness");
        args.push_back(std::to_string(options.personality.conscientiousness));
        args.emplace_back("--extraversion");
        args.push_back(std::to_string(options.personality.extraversion));
        args.emplace_back("--agreeableness");
        args.push_back(std::to_string(options.personality.agreeableness));
        args.emplace_back("--neuroticism");
        args.push_back(std::to_string(options.personality.neuroticism));
        args.emplace_back("--quirk");
        args.push_back(options.personality.quirk);
    }
    return args;
}

#ifdef _WIN32
std::string quoteArg(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') {
            out += '\\';
        }
        out += ch;
    }
    out += '"';
    return out;
}
#endif
} // namespace

namespace SpawnProcess {
long spawnHeadlessAI(const SpawnOptions& options, bool mock_llm) {
    const std::string exe = executablePath();
    const auto args = buildArgs(exe, options, mock_llm);
#ifdef _WIN32
    std::string command;
    for (const auto& arg : args) {
        if (!command.empty()) {
            command += ' ';
        }
        command += quoteArg(arg);
    }
    // NUL is the Windows equivalent of /dev/null.
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE null_handle =
        CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, nullptr);
    if (null_handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("could not open NUL for child stdio");
    }
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nullptr;
    startup.hStdOutput = null_handle;
    startup.hStdError = null_handle;
    PROCESS_INFORMATION info{};
    const BOOL ok = CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                   &startup, &info);
    CloseHandle(null_handle);
    if (!ok) {
        throw std::runtime_error("could not spawn AI process");
    }
    const long pid = static_cast<long>(info.dwProcessId);
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    return pid;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("could not fork AI process");
    }
    if (pid == 0) {
        const int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) {
                close(devnull);
            }
        }
        std::vector<std::string> owned = args;
        std::vector<char*> argv;
        argv.reserve(owned.size() + 1);
        // cppcheck-suppress constVariableReference
        for (auto& arg : owned) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        execvp(argv.front(), argv.data());
        _exit(127);
    }
    return static_cast<long>(pid);
#endif
}

void terminate(long pid) {
    if (pid <= 0) {
        return;
    }
#ifdef _WIN32
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (handle != nullptr) {
        TerminateProcess(handle, 0);
        CloseHandle(handle);
    }
#else
    kill(static_cast<pid_t>(pid), SIGTERM);
#endif
}
} // namespace SpawnProcess
