#include "calibration.hpp"

namespace servo {
  Calibration::Point::Point()
  : pulse(0.0f), value(0.0f) {
  }

  Calibration::Point::Point(float pulse, float value)
  : pulse(pulse), value(value) {
  }

  Calibration::Calibration()
    : calibration(nullptr), calibration_size(0), limit_lower(true), limit_upper(true) {
  }

  Calibration::Calibration(CalibrationType default_type)
    : Calibration() {
    apply_default(default_type);
  }

  Calibration::Calibration(const Calibration &other)
    : calibration(nullptr), calibration_size(0), limit_lower(other.limit_lower), limit_upper(other.limit_upper) {
    uint size = other.size();
    apply_blank(size);
    for(uint i = 0; i < size; i++) {
      calibration[i] = other.calibration[i];
    }
  }

  Calibration::~Calibration() {
    if(calibration != nullptr) {
      delete[] calibration;
      calibration = nullptr;
    }
  }

  Calibration& Calibration::operator=(const Calibration &other) {
    uint size = other.size();
    apply_blank(size);
    for(uint i = 0; i < size; i++) {
      calibration[i] = other.calibration[i];
    }
    limit_lower = other.limit_lower;
    limit_upper = other.limit_upper;

    return *this;
  }

  Calibration::Point& Calibration::operator[](uint8_t index) const {
    return calibration[index];
  }

  void Calibration::apply_blank(uint size) {
    if(calibration != nullptr) {
      delete[] calibration;
    }

    if(size > 0) {
      calibration = new Point[size];
      calibration_size = size;
    }
    else {
      calibration = nullptr;
      calibration_size = 0;
    }
  }

  void Calibration::apply_two_point(float min_pulse, float max_pulse, float min_value, float max_value) {
    apply_blank(2);
    calibration[0] = Point(min_pulse, min_value);
    calibration[1] = Point(max_pulse, max_value);
  }

  void Calibration::apply_three_point(float min_pulse, float mid_pulse, float max_pulse, float min_value, float mid_value, float max_value) {
    apply_blank(3);
    calibration[0] = Point(min_pulse, min_value);
    calibration[1] = Point(mid_pulse, mid_value);
    calibration[2] = Point(max_pulse, max_value);
  }

  void Calibration::apply_uniform(uint size, float min_pulse, float max_pulse, float min_value, float max_value) {
    apply_blank(size);
    if(size > 0) {
      float size_minus_one = (float)(size - 1);
      for(uint i = 0; i < size; i++) {
        float pulse = Calibration::map_float((float)i, 0.0f, size_minus_one, min_pulse, max_pulse);
        float value = Calibration::map_float((float)i, 0.0f, size_minus_one, min_value, max_value);
        calibration[i] = Point(pulse, value);
      }
    }
  }

  void Calibration::apply_default(CalibrationType default_type) {
    switch(default_type) {
    default:
    case ANGULAR:
      apply_three_point(DEFAULT_MIN_PULSE, DEFAULT_MID_PULSE, DEFAULT_MAX_PULSE,
                                    -90.0f,            0.0f,              +90.0f);
      break;
    case LINEAR:
      apply_two_point(DEFAULT_MIN_PULSE, DEFAULT_MAX_PULSE,
                                  0.0f,              1.0f);
      break;
    case CONTINUOUS:
      apply_three_point(DEFAULT_MIN_PULSE, DEFAULT_MID_PULSE, DEFAULT_MAX_PULSE,
                                    -1.0f,            0.0f,              +1.0f);
      break;
    }
  }

  uint Calibration::size() const {
    return calibration_size;
  }

  Calibration::Point* Calibration::point_at(uint8_t index) const {
    if(index < calibration_size) {
      return &calibration[index];
    }
    return nullptr;
  }

  Calibration::Point* Calibration::first_point() const {
    if(calibration_size > 0) {
      return &calibration[0];
    }
    return nullptr;
  }

  Calibration::Point* Calibration::last_point() const {
    if(calibration_size > 0) {
      return &calibration[calibration_size - 1];
    }
    return nullptr;
  }

  bool Calibration::has_lower_limit() const {
    return limit_lower;
  }

  bool Calibration::has_upper_limit() const {
    return limit_upper;
  }

  void Calibration::limit_to_calibration(bool lower, bool upper) {
    limit_lower = lower;
    limit_upper = upper;
  }

  bool Calibration::value_to_pulse(float value, float &pulse_out, float &value_out) const {
    bool success = false;
    if(calibration_size >= 2) {
      uint8_t last = calibration_size - 1;

      value_out = value;

      // Is the value below the bottom most calibration point?
      if(value < calibration[0].value) {
        // Should the value be limited to the calibration or projected below it?
        if(limit_lower) {
          pulse_out = calibration[0].pulse;
          value_out = calibration[0].value;
        }
        else {
          pulse_out = map_float(value, calibration[0].value, calibration[1].value,
                                       calibration[0].pulse, calibration[1].pulse);
        }
      }
      // Is the value above the top most calibration point?
      else if(value > calibration[last].value) {
        // Should the value be limited to the calibration or projected above it?
        if(limit_upper) {
          pulse_out = calibration[last].pulse;
          value_out = calibration[last].value;
        }
        else {
          pulse_out = map_float(value, calibration[last - 1].value, calibration[last].value,
                                       calibration[last - 1].pulse, calibration[last].pulse);
        }
      }
      else {
        // The value must between two calibration points, so iterate through them to find which ones
        for(uint8_t i = 0; i < last; i++) {
          if(value <= calibration[i + 1].value) {
            pulse_out = map_float(value, calibration[i].value, calibration[i + 1].value,
                                         calibration[i].pulse, calibration[i + 1].pulse);
            break; // No need to continue checking so break out of the loop
          }
        }
      }

      // Clamp the pulse between the hard limits
      if(pulse_out < LOWER_HARD_LIMIT || pulse_out > UPPER_HARD_LIMIT) {
        pulse_out = MIN(MAX(pulse_out, LOWER_HARD_LIMIT), UPPER_HARD_LIMIT);

        // Is the pulse below the bottom most calibration point?
        if(pulse_out < calibration[0].pulse) {
          value_out = map_float(pulse_out, calibration[0].pulse, calibration[1].pulse,
                                           calibration[0].value, calibration[1].value);
        }
        // Is the pulse above the top most calibration point?
        else if(pulse_out > calibration[last].pulse) {
          value_out = map_float(pulse_out, calibration[last - 1].pulse, calibration[last].pulse,
                                           calibration[last - 1].value, calibration[last].value);
        }
        else {
          // The pulse must between two calibration points, so iterate through them to find which ones
          for(uint8_t i = 0; i < last; i++) {
            if(pulse_out <= calibration[i + 1].pulse) {
              value_out = map_float(pulse_out, calibration[i].pulse, calibration[i + 1].pulse,
                                               calibration[i].value, calibration[i + 1].value);
              break; // No need to continue checking so break out of the loop
            }
          }
        }
      }

      success = true;
    }

    return success;
  }

  bool Calibration::pulse_to_value(float pulse, float &value_out, float &pulse_out) const {
    bool success = false;
    if(calibration_size >= 2) {
      uint8_t last = calibration_size - 1;

      // Clamp the pulse between the hard limits
      pulse_out = MIN(MAX(pulse, LOWER_HARD_LIMIT), UPPER_HARD_LIMIT);

      // Is the pulse below the bottom most calibration point?
      if(pulse_out < calibration[0].pulse) {
        // Should the pulse be limited to the calibration or projected below it?
        if(limit_lower) {
          value_out = calibration[0].value;
          pulse_out = calibration[0].pulse;
        }
        else {
          value_out = map_float(pulse, calibration[0].pulse, calibration[1].pulse,
                                       calibration[0].value, calibration[1].value);
        }
      }
      // Is the pulse above the top most calibration point?
      else if(pulse > calibration[last].pulse) {
        // Should the pulse be limited to the calibration or projected above it?
        if(limit_upper) {
          value_out = calibration[last].value;
          pulse_out = calibration[last].pulse;
        }
        else {
          value_out = map_float(pulse, calibration[last - 1].pulse, calibration[last].pulse,
                                       calibration[last - 1].value, calibration[last].value);
        }
      }
      else {
        // The pulse must between two calibration points, so iterate through them to find which ones
        for(uint8_t i = 0; i < last; i++) {
          if(pulse <= calibration[i + 1].pulse) {
            value_out = map_float(pulse, calibration[i].pulse, calibration[i + 1].pulse,
                                         calibration[i].value, calibration[i + 1].value);
            break; // No need to continue checking so break out of the loop
          }
        }
      }

      success = true;
    }

    return success;
  }

  float Calibration::map_float(float in, float in_min, float in_max, float out_min, float out_max) {
    return (((in - in_min) * (out_max - out_min)) / (in_max - in_min)) + out_min;
  }
};