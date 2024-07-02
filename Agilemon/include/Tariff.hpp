#pragma once
#include <cstdint>
#include <cmath>

/** Price stored as Float s8p8
*/
class Price
{
public:
    //TODO: Fixed8p8 type (8 bits integer, 8 bits decimal etc)
    static constexpr int16_t Scale = 256;

    constexpr Price() : value_() {}
    constexpr Price(float f) : value_( static_cast<int16_t>(std::round(f * Scale)) ) {}
    constexpr Price(const Price& rhs) = default;

    constexpr operator float() const
    {
      return value_ * (1.0F/Scale); 
    }

private:
    int16_t value_;
};

static_assert(Price(100.0f) == 100.0F);
static_assert(Price(0.5f) == 0.5F);


/** 30-minute time resolution 
*/
class Time
{
public:
  static constexpr uint32_t Minute = 60; //Hour in seconds
  static constexpr uint32_t Hour = Minute * 60; //Hour in seconds
  static constexpr uint32_t HalfHour = Hour/2; //half Hour in seconds (=1800)
  static constexpr uint32_t Day = Hour*24; //half Hour in seconds (=1800)
  static constexpr time_t EpochOffset = 1577836800LL; // time_t offset from 00:00 1-1-1970 to 00:00 1-1-2020

  struct Internal{};

  static Time nowUTC() 
  {
     // @note time() Returns the time as the number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
    return { time(NULL) };
  }

  constexpr Time() : value_() {}
  constexpr Time(time_t t) : value_( t - EpochOffset ) {}
  constexpr Time(const Time& rhs) = default;
  constexpr Time(uint32_t value, Internal ) : value_(value) {}

  constexpr time_t toPosix() const
  {
      return static_cast<time_t>(value_) + EpochOffset;
  }
  
  constexpr operator uint32_t() const
  {
      return value_;
  }
 
  Time roundUp( uint32_t toMul = HalfHour ) const  { return { ((value_ + (toMul-1)) / toMul) * toMul, Internal{} }; }  
  Time round( uint32_t toMul = HalfHour) const  { return { ((value_ + (toMul/2)) / toMul) * toMul, Internal{} }; }
  Time roundDown( uint32_t toMul = HalfHour) const  { return { (value_ / toMul) * toMul, Internal{} }; }

private:
    uint32_t value_;
};

/** Tariff datas
@todo If we need to olptimise space, we could reduce storing just StartTime(32bit) and deltaT(8bit) offset between each change of tarriff
*/
struct Tariff
{
  
    // Next day is published between 1600-2000 for the next "24-hour period" but actually until 22:30 the next day
    // 23 + (24 - 16) = 31 hours = 62 halfHours 
    // @note We round upto 64 as a resounable safe count
    static constexpr uint8_t MaxRecords = 64;

    Time startTimes[MaxRecords];
    Price prices[MaxRecords];
    
    uint8_t numRecords = 0; // No. of tariff records available from Octopus API
};