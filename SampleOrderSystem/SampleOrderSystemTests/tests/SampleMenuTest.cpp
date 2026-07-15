#include "testing.h"

#include <filesystem>
#include <sstream>

#include "Console/ConsoleIO.h"
#include "Console/SampleMenu.h"
#include "Repository/SampleRepository.h"

namespace {

std::filesystem::path testFile(const std::string& name) {
    return std::filesystem::path("test_data") / name;
}

}  // namespace

TEST(sampleMenu_registerThenListShowsSample) {
    const auto path = testFile("sample_menu_register.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    std::istringstream in(
        "1\n"
        "WaferA\n"
        "10\n"
        "0.9\n"
        "2\n"
        "0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    SampleMenu menu(repo, io);
    menu.run();

    EXPECT_TRUE(out.str().find("WaferA") != std::string::npos);
    EXPECT_EQ(repo.findAll().size(), static_cast<size_t>(1));

    std::filesystem::remove(path);
}

TEST(sampleMenu_registerRejectsInvalidYield) {
    const auto path = testFile("sample_menu_invalid.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    std::istringstream in(
        "1\n"
        "BadWafer\n"
        "10\n"
        "1.5\n"
        "0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    SampleMenu menu(repo, io);
    menu.run();

    EXPECT_TRUE(repo.findAll().empty());

    std::filesystem::remove(path);
}

TEST(sampleMenu_searchFindsByPartialName) {
    const auto path = testFile("sample_menu_search.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);
    repo.create("WaferAlpha", 10.0, 0.9);

    std::istringstream in(
        "3\n"
        "alpha\n"
        "0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    SampleMenu menu(repo, io);
    menu.run();

    EXPECT_TRUE(out.str().find("WaferAlpha") != std::string::npos);

    std::filesystem::remove(path);
}

TEST(sampleMenu_registerRepromptsOnEmptyName) {
    const auto path = testFile("sample_menu_empty_name.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    std::istringstream in(
        "1\n"
        "\n"          // 빈 이름 -> 재입력 요구
        "WaferA\n"
        "10\n"
        "0.9\n"
        "0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    SampleMenu menu(repo, io);
    menu.run();

    EXPECT_EQ(repo.findAll().size(), static_cast<size_t>(1));
    EXPECT_EQ(repo.findAll()[0].name, "WaferA");

    std::filesystem::remove(path);
}

TEST(sampleMenu_listShowsEmptyMessageWhenNoSamples) {
    const auto path = testFile("sample_menu_empty.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    std::istringstream in(
        "2\n"
        "0\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    SampleMenu menu(repo, io);
    menu.run();

    EXPECT_TRUE(out.str().find("등록된 시료가 없습니다.") != std::string::npos);

    std::filesystem::remove(path);
}

TEST(sampleMenu_eofDuringMenuReturnsWithoutThrowing) {
    const auto path = testFile("sample_menu_eof.json");
    std::filesystem::remove(path);
    SampleRepository repo(path);

    std::istringstream in("2\n");  // 조회 실행 후 다음 선택 프롬프트에서 EOF
    std::ostringstream out;
    ConsoleIO io(in, out);
    SampleMenu menu(repo, io);

    menu.run();  // 예외가 전파되지 않고 정상 반환되어야 함
    EXPECT_TRUE(true);

    std::filesystem::remove(path);
}
