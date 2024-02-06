
#include "test_utils.hpp"

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	AddGlobalTestEnvironment(new FileshareTestEnvironment(argc, argv));
	return RUN_ALL_TESTS();
}
