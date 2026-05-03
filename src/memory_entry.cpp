#include "memory_entry.h"

#include <utility>

MemoryEntry::MemoryEntry(std::string text) : text_(std::move(text)) {}

const std::string& MemoryEntry::getText() const {
    return text_;
}

std::string JournalEntry::getKind() const {
    return "journal";
}

std::unique_ptr<MemoryEntry> JournalEntry::clone() const {
    return std::make_unique<JournalEntry>(*this);
}

LetterEntry::LetterEntry(std::string from, std::string to, std::string body)
    : MemoryEntry(std::move(body)), from_(std::move(from)), to_(std::move(to)) {}

const std::string& LetterEntry::getFrom() const {
    return from_;
}

const std::string& LetterEntry::getTo() const {
    return to_;
}

std::string LetterEntry::getKind() const {
    return "letter";
}

std::unique_ptr<MemoryEntry> LetterEntry::clone() const {
    return std::make_unique<LetterEntry>(*this);
}

std::string LastWords::getKind() const {
    return "last_words";
}

std::unique_ptr<MemoryEntry> LastWords::clone() const {
    return std::make_unique<LastWords>(*this);
}

Inheritance::Inheritance(std::string from, std::string body)
    : MemoryEntry(std::move(body)), from_(std::move(from)) {}

const std::string& Inheritance::getFrom() const {
    return from_;
}

std::string Inheritance::getKind() const {
    return "inheritance";
}

std::unique_ptr<MemoryEntry> Inheritance::clone() const {
    return std::make_unique<Inheritance>(*this);
}
