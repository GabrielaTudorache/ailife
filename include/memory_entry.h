#ifndef AILIFE_MEMORY_ENTRY_H
#define AILIFE_MEMORY_ENTRY_H

#include <memory>
#include <string>

class MemoryEntry {
  public:
    explicit MemoryEntry(std::string text);
    virtual ~MemoryEntry() = default;

    const std::string& getText() const;
    virtual std::string getKind() const = 0;
    virtual std::unique_ptr<MemoryEntry> clone() const = 0;

  private:
    std::string text_;
};

class JournalEntry : public MemoryEntry {
  public:
    using MemoryEntry::MemoryEntry;
    std::string getKind() const override;
    std::unique_ptr<MemoryEntry> clone() const override;
};

class LetterEntry : public MemoryEntry {
  public:
    LetterEntry(std::string from, std::string to, std::string body);
    const std::string& getFrom() const;
    const std::string& getTo() const;
    std::string getKind() const override;
    std::unique_ptr<MemoryEntry> clone() const override;

  private:
    std::string from_;
    std::string to_;
};

class LastWords : public MemoryEntry {
  public:
    using MemoryEntry::MemoryEntry;
    std::string getKind() const override;
    std::unique_ptr<MemoryEntry> clone() const override;
};

class Inheritance : public MemoryEntry {
  public:
    Inheritance(std::string from, std::string body);
    const std::string& getFrom() const;
    std::string getKind() const override;
    std::unique_ptr<MemoryEntry> clone() const override;

  private:
    std::string from_;
};

#endif // AILIFE_MEMORY_ENTRY_H
