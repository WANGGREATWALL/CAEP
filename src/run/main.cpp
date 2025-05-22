#include "gtest/gtest.h"
#include "caep.h"

#define TAG_LOGGER "[CAEP]"
#include "logger.h"
volatile int G_LEVEL_LOGGER = LEVEL_LOGGER_DEFAULT;


void showTitle()
{
    LOGGER_I("************************************************\n");
    LOGGER_I("*\n");
    LOGGER_I("* CAEP v%s\n", CAEP_VERSION);
    LOGGER_I("*\n");
    LOGGER_I("************************************************\n\n");
}

int main(int argc, char **argv)
{
    showTitle();

    testing::InitGoogleTest(&argc, argv);
    int retGTest = RUN_ALL_TESTS();
    ASSERTER_WITH_RET(retGTest == NO_ERROR, retGTest);

    // caep
    int retDemoHole = demo_hole();
    ASSERTER_WITH_RET(retDemoHole == NO_ERROR, retDemoHole);

    return NO_ERROR;
}