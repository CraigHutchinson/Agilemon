#pragma once


/** 24-hour clock time as HH:MM
*/
struct Time24
{
    uint8_t hour;
    uint8_t minute;

    /** Duration as clock time HH:MM where the hours are not wrapped after 24 hours e.g. 35:59 for example for > 1 day
    */
    static Time24 fromSecondsDuration( uint32_t durationSeconds )
    {
        const auto durationMinutes = ((durationSeconds + 30) / 60); //< Round to nearest minute
        uint8_t hour = static_cast<uint8_t>(durationMinutes / 60);
        uint8_t minute = static_cast<uint8_t>(durationMinutes - (hour * 60));
        return { hour, minute };
    }

    /** Duration as clock time HH:MM within the 24-hour clock period from 0:00 to 23:59
    */
    static Time24 fromSecondsTimepoint( uint32_t timepointSeconds )
    {
      auto t = fromSecondsDuration(timepointSeconds);
      if ( t.hour >= 24 )
        t.hour -= (t.hour/24) * 24; //< Wrap hour on 24 hour clock
      return t;
    }
};