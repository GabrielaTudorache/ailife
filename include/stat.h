#ifndef AILIFE_STAT_H
#define AILIFE_STAT_H

template <typename T> class Stat {
  public:
    Stat() = default;
    Stat(T value, T min_value, T max_value, T decay_per_tick)
        : value_(value), min_value_(min_value), max_value_(max_value), decay_per_tick_(decay_per_tick) {
        clamp();
    }

    T getValue() const { return value_; }
    T getMin() const { return min_value_; }
    T getMax() const { return max_value_; }

    void decay() { modify(-decay_per_tick_); }

    void modify(T delta) {
        value_ += delta;
        clamp();
    }

    bool isCritical(T threshold) const { return value_ <= threshold; }

    Stat& operator+=(T delta) {
        modify(delta);
        return *this;
    }

  private:
    void clamp() {
        if (value_ < min_value_) {
            value_ = min_value_;
        }
        if (value_ > max_value_) {
            value_ = max_value_;
        }
    }

    T value_{};
    T min_value_{};
    T max_value_{};
    T decay_per_tick_{};
};

#endif // AILIFE_STAT_H
