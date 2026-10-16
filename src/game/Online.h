#pragma once
// Sending games to the online high scores, which only the website shows. A game is sent when
// the player, having typed initials for a high score, says yes to it; what cannot be sent now
// waits in replays/outbox and goes with the next one, or at the next start.
//
// The installation is known to the server by a secret token made here the first time a game
// is sent and kept in online.txt beside the high scores, and to everyone else by the tag the
// server gives it, shown beside the initials on the website.
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace encore {

/// The server; ENCORE_API points the game at another, to try one locally.
std::string onlineApi();

class ScoreSender {
 public:
  explicit ScoreSender(std::filesystem::path saveDir);
  ~ScoreSender();
  ScoreSender(const ScoreSender&) = delete;
  ScoreSender& operator=(const ScoreSender&) = delete;

  /// Puts a copy of a recording in the outbox.
  void queue(const std::filesystem::path& recording);
  /// Sends what is in the outbox, in the background, unless that is already happening.
  void send();
  /// Whether nothing is being sent now.
  bool idle() const { return !busy_->load(); }
  /// Waits for the sending thread, once it is idle (the outbox's files are then closed and the
  /// folder can be moved).
  void join() {
    if (thread_.joinable() && idle()) thread_.join();
  }

 private:
  std::filesystem::path saveDir_;
  std::thread thread_;
  std::shared_ptr<std::atomic<bool>> busy_ = std::make_shared<std::atomic<bool>>(false);
};

}  // namespace encore
