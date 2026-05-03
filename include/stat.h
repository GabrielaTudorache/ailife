#ifndef AILIFE_STAT_H
#define AILIFE_STAT_H

template <typename T> class Stat {
  public:
    Stat() = default;
    Stat(T value, T max_value) : value_(value), max_value_(max_value) {}

    T getValue() const { return value_; }
    T getMax() const { return max_value_; }

    Stat& operator+=(T delta) {
        value_ += delta;
        if (value_ > max_value_) {
            value_ = max_value_;
        }
        return *this;
    }

  private:
    T value_{};
    T max_value_{};
};

#endif // AILIFE_STAT_H
