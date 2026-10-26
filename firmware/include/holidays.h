#pragma once
// 中国法定节假日（2026，国办发明电〔2025〕7号）与调休上班日。
// 每年国务院公布后更新 src/core/holidays.cpp 里的两张表。
namespace holidays {

bool isHoliday(int year, int mon, int day);        // 法定放假日
bool isMakeupWorkday(int year, int mon, int day);  // 调休上班的周末
bool isWorkday(int year, int mon, int day);        // 周一~周五且非节假日，或调休上班日

}  // namespace holidays
