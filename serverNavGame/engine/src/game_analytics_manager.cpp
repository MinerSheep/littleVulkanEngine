#include <game_analytics_manager.hpp>

#include "GameAnalytics/GameAnalytics.h"

// #include "version.h"
#include <assert.h>

#include <iostream>
#include <string>

std::string GameAnalyticsManager::GameKey() { return "b88854b0e543ea056b41b57203c31a6e"; }

std::string GameAnalyticsManager::SecretKey() { return "c50090e37e72ef97c7358a47c6457f68ba70cf7c"; }

GameAnalyticsManager& GameAnalyticsManager::Get() {
  static GameAnalyticsManager instance;
  return instance;
}

void GameAnalyticsManager::Initialize() {
  std::vector<std::string> currencies{"map", "cue", "spade", "key", "piece"};
  gameanalytics::GameAnalytics::configureAvailableResourceCurrencies(currencies);

  std::vector<std::string> itemTypes{"item"};  // whatever categories you actually want
  gameanalytics::GameAnalytics::configureAvailableResourceItemTypes(itemTypes);

  gameanalytics::GameAnalytics::configureBuild("PROJECT_VERSION");
  gameanalytics::GameAnalytics::initialize(GameKey(), SecretKey());
}

void GameAnalyticsManager::ReloadActiveSteps(std::set<std::string> state) {
  m_completedSteps = state;
}

void GameAnalyticsManager::StartLevel() { ProgressionStarted(levelName, "Progress"); }

void GameAnalyticsManager::CompleteLevel() { ProgressionCompleted(levelName, "Progress");}

void GameAnalyticsManager::StepStarted(const std::string& step) {
  if (m_activeSteps.count(step) || m_completedSteps.count(step)) {
    std::cout << "Step already started or completed" << step << "\n";
    return;
  }
  m_activeSteps.insert(step);
  ProgressionStarted(levelName, "Progress", step);
}

void GameAnalyticsManager::StepCompleted(const std::string& step) {
  if (!m_activeSteps.count(step)) {
    std::cout << "Step not started or completed" << step << "\n";
    StepStarted(step);
  }
  m_activeSteps.erase(step);
  m_completedSteps.insert(step);
  ProgressionCompleted(levelName, "Progress", step);
}

void GameAnalyticsManager::ProgressionStarted(
    const std::string& world, const std::string& level, const std::string& step) {
  if (step == "")
    gameanalytics::GameAnalytics::addProgressionEvent(
        gameanalytics::EGAProgressionStatus::Start,
        world,
        level);
  else
    gameanalytics::GameAnalytics::addProgressionEvent(
        gameanalytics::EGAProgressionStatus::Start,
        world,
        level,
        step);
}

void GameAnalyticsManager::ProgressionCompleted(
    const std::string& world, const std::string& level, const std::string& step) {
  if (step == "")
    gameanalytics::GameAnalytics::addProgressionEvent(
        gameanalytics::EGAProgressionStatus::Complete,
        world,
        level);
  else
    gameanalytics::GameAnalytics::addProgressionEvent(
        gameanalytics::EGAProgressionStatus::Complete,
        world,
        level,
        step);
}

void GameAnalyticsManager::ItemEvent(
    bool found,
    std::string const& item,
    float amount,
    std::string const& itemType,
    std::string const& itemId) {
  gameanalytics::GameAnalytics::addResourceEvent(
      gameanalytics::EGAResourceFlowType::Source,
      item,
      amount,
      itemType,
      itemId);
}

void GameAnalyticsManager::PlayerEvent(const std::string& room, const std::string& action) {
  // Builds a hierarchical event id like "Event:Foyer:Tree_Taken"
  // GA groups these in the dashboard by colon-separated tier, same idea
  // as your progression tiers, just without the Start/Complete state.
  std::string eventId = "Event:" + room + ":" + action;
  gameanalytics::GameAnalytics::addDesignEvent(eventId);
}