/**
 * @file test_main.cpp
 * @brief Entry point for unit tests — init logging then run GTest
 */

#include "core/LogManager.h"
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    kenga::LogManager::init();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
