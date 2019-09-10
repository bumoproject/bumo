#include <gtest/gtest.h>
#include <utils/net.h>
#include <utils/logger.h>

GTEST_API_ int main(int argc, char **argv){
	utils::net::Initialize();
	utils::Logger::InitInstance();
	utils::Logger &logger = utils::Logger::Instance();
	logger.Initialize(utils::LOG_DEST_ALL, utils::LOG_LEVEL_ALL, "", false);

	//testing::AddGlobalTestEnvironment(new FooEnvironment);
	testing::GTEST_FLAG(output) = "xml:gtest_result.xml";
	testing::InitGoogleTest(&argc, argv);
	RUN_ALL_TESTS();
	printf("enter for exit\n");
	getchar();
	return 0;
}
