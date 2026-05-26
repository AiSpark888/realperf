#include <catch2/catch_test_macros.hpp>
#include "realperf/CheckPoint.hpp"

TEST_CASE("Recorder can add checkpoints and commit them")
{
    realperf::CheckPoint buffer[8];
    realperf::Recorder recorder;
    recorder.init(buffer, buffer + 8);

    recorder.add("first", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_MD);
    recorder.add("second", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Order);
    recorder.add("third", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Strategy);

    CHECK(recorder.has(realperf::Category::CAT_MD));
    CHECK(recorder.has(realperf::Category::CAT_Order));
    CHECK(recorder.has(realperf::Category::CAT_Strategy));
    CHECK_FALSE(recorder.has(realperf::Category::CAT_Default));

    recorder.commit();

    CHECK(buffer[0].where_ == "first");
    CHECK(buffer[0].category_ == realperf::Category::CAT_MD);
    CHECK(buffer[1].where_ == "second");
    CHECK(buffer[1].category_ == realperf::Category::CAT_Order);
    CHECK(buffer[2].where_ == "third");
    CHECK(buffer[2].category_ == realperf::Category::CAT_Strategy);
}