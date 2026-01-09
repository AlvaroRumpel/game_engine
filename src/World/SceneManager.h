#pragma once
#include <string>

class SceneManager
{
public:
    void setCurrentScenePath(const std::string &path) { currentScenePath_ = path; }
    const std::string &currentScenePath() const { return currentScenePath_; }

    void setProjectRoot(const std::string &root) { projectRoot_ = root; }
    const std::string &projectRoot() const { return projectRoot_; }

private:
    std::string currentScenePath_;
    std::string projectRoot_;
};
