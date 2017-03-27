#include "wtf/Time.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

TEST(TimeTicks, NumericOperations) {
  TimeTicks zero;
  ASSERT_EQ(0, zero.ToInternalValueForTesting());

  TimeTicks t1 = zero + TimeDelta::FromMilliseconds(1001);
  ASSERT_EQ(1001000, t1.ToInternalValueForTesting());

  TimeTicks t2 = zero + TimeDelta::FromSeconds(1);
  ASSERT_EQ(TimeDelta::FromMilliseconds(1), t1 - t2);
}

TEST(Time, NumericOperations) {
  Time zero;
  ASSERT_EQ(0, zero.ToInternalValueForTesting());

  Time t1 = zero + TimeDelta::FromMilliseconds(1001);
  ASSERT_EQ(1001000, t1.ToInternalValueForTesting());

  Time t2 = zero + TimeDelta::FromSeconds(1);
  ASSERT_EQ(TimeDelta::FromMilliseconds(1), t1 - t2);
}

}  // namespace
}  // namespace WTF
