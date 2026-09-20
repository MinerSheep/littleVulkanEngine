#pragma once

#include <set>
#include <string>

enum class LevelStep { SearchMirror, MoveStone, DigYard, Ballroom };

class GameAnalyticsManager {
    public:
    static GameAnalyticsManager& Get();
    
    std::string GameKey();
    std::string SecretKey();
    
    void Initialize();
    void ReloadActiveSteps(std::set<std::string> state);
    
    std::string levelName = "Petscop";
  void StartLevel();
  void CompleteLevel(); //LevelCompletedIfDone();  // call after every step completes; fires once all 4 are done

  void StepStarted(const std::string& step);
  void StepCompleted(const std::string& step);

  void ProgressionStarted(
      const std::string& world, const std::string& level, const std::string& step = "");

  void ProgressionCompleted(
      const std::string& world, const std::string& level, const std::string& step = "");

  void ItemEvent(
      bool found,
      std::string const& currency,
      float amount,
      std::string const& itemType,
      std::string const& itemId);

  void PlayerEvent(const std::string& room, const std::string& action);

  // void LevelFailed(const std::string& world, const std::string& level);

 private:
  GameAnalyticsManager() = default;


  std::set<std::string> m_activeSteps;
  std::set<std::string> m_completedSteps;
};