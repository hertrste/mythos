#pragma once

#include <array>
#include "app/ISignalable.hh"
#include "app/Mutex.hh"
#include "app/mlog.hh"
#include "app/Thread.hh"
#include "app/ThreadManager.hh"
#include "app/Task.hh"

class SignalGroup {
  public:
    enum STRATEGY {
      SEQUENTIAL = 0,
      TREE,
      HELPER,
    };
  public:
    void addMember(ISignalable *t);
    void signalAll();
    uint64_t count() { return size; }
    ISignalable* getMember(uint64_t i) { ASSERT(i < size); return member[i]; }
    Task* getTask(uint64_t i) { ASSERT(i < size); return &tasks[i]; }
    void setStrat(STRATEGY strat_) { strat = strat_; }
    void setAvailableHelper(uint64_t helper) { ASSERT(helper < NUM_HELPER); availableHelper_ = helper; }
    uint64_t getAvailableHelper() { return availableHelper_; }
  private:
    static const size_t MAX = 256;
    std::array<ISignalable*, MAX> member{{nullptr}};
    std::array<Task, MAX> tasks;
    uint64_t size {0};
    uint64_t availableHelper_ = NUM_HELPER;
    STRATEGY strat = {SEQUENTIAL};
};

class SequentialStrategy {
  public:
    static void cast(SignalGroup *group, uint64_t idx, uint64_t size) {
      for (uint64_t i = idx; i < size; ++i) {
        auto *member = group->getMember(i);
        if (member) member->signal();
      }
    }
};

extern const size_t NUM_HELPER;
extern ThreadManager manager;
class HelperStrategy {
  public:

    static void simple_cast(SignalGroup *group, uint64_t idx, uint64_t size) {
      ASSERT(group != nullptr);
      uint64_t availableHelper = group->getAvailableHelper();
      if (availableHelper == 0) {
        return;
      }
      auto perHelper = size / availableHelper;
      auto rest      = size % availableHelper;
      auto current   = 0ul;

      for (uint64_t i = 0; i < availableHelper; i++) {
        auto toHandle = perHelper;
        if (i == 0) toHandle += rest; // first helper gets rest
        if (toHandle == 0) break;
        auto *tasklet = group->getTask(i);
        //MLOG_ERROR(mlog::boot,availableHelper, "send to Helper Thread:", i, "from", current, "to",current + toHandle);
        tasklet->set([group, current, toHandle](Task&) {
            ASSERT(group != nullptr);
            //MLOG_ERROR(mlog::boot, "Signal from", current, current+toHandle);
            for (uint64_t j = current; j < current + toHandle; j++) {
              ISignalable* dest = group->getMember(j);
              if (dest) {
                dest->signal();
              } else {
                PANIC("Could not reach signalable.");
              }
            }
          });
        auto helper = manager.getThread(manager.getNumThreads() - i - 1);
        helper->addTask(&tasklet->list_member);
        helper->signal();
        current += toHandle;
      }
    }

    static void cast(SignalGroup *group, uint64_t idx, uint64_t size) {
      if (size == 0) return;
      ASSERT(group != nullptr);
      uint64_t threads = size/* - 1*/;
      uint64_t availableHelper = group->getAvailableHelper();
      if (availableHelper == 0) {
        PANIC("No Resources");
      }
      uint64_t optimalHelper = num_helper(threads);
      uint64_t usedHelper = (availableHelper < optimalHelper) ? availableHelper : optimalHelper;
      uint64_t optimalThreadNumber = (usedHelper * (usedHelper + 1)) / 2;
      int64_t  diffThreads = threads - optimalThreadNumber;
      uint64_t current = 0;
      uint64_t diff = diffThreads / usedHelper;
      uint64_t mod = diffThreads % usedHelper; // never bigger than usedHelper


      for (uint64_t i = 0; i < usedHelper; i++) {

        uint64_t base = usedHelper - i;
        if (diffThreads > 0) {
          uint64_t tmp = diff;
          if (mod > 0) {
            tmp++;
            mod--;
          }
          base += tmp;
          diffThreads -= tmp;
        }

        auto *tasklet = group->getTask(i); // use a tasklet from SignalingGroup, should not be in use
        //MLOG_ERROR(mlog::boot, "send to Helper Thread:", DVAR(current), DVAR(current + base-1));
        tasklet->set([group, i, current, base](Task&) {
            ASSERT(group != nullptr);
            for (uint64_t i = current; i < current + base; i++) {
            //MLOG_ERROR(mlog::app, "In Helper",i, DVAR(current), DVAR(base));
            ISignalable* dest = group->getMember(i);
            if (dest) {
            dest->signal();
            } else {
            PANIC("Could not reach signalable.");
            }
            }
            });

        //MLOG_ERROR(mlog::app, "Helper", manager.getNumThreads() - i - 1, "from", current, "to", current + base);
        auto helper = manager.getThread(manager.getNumThreads() - i - 1);
        helper->addTask(&tasklet->list_member);
        helper->signal();
        current += base;
      }
    }

  private:
    // Calculates optimal number of helper threads for given number of threads
    static uint64_t num_helper(uint64_t groupSize) {
      uint64_t value {0};
      uint64_t i {0};
      while (value <= groupSize) {
        i++;
        value = (i * (i + 1)) / 2;
      }
      return (i - 1 > 0) ? i - 1 : 0;
    }
};

/**
 * Tree Strategy
 */
class TreeStrategy {
  private:
    static const uint64_t LATENCY = 3;
  public:
    static const  uint64_t RECURSIVE_SIZE = 25;
    static int64_t tmp[RECURSIVE_SIZE]; //recursive memory

    // Helper functions to calculate fibonacci tree for optimal multicast
    // saves result of previous calcs to optimize expensive recursion
    static size_t F(size_t time) {
      if (time < LATENCY) {
        return 1;
      }
      if (time < RECURSIVE_SIZE && tmp[time] > 0) {
        return tmp[time];
      }
      auto ret = F(time - 1) + F(time - LATENCY);
      if (time < RECURSIVE_SIZE) {
        tmp[time] = ret;
      }
      return ret;
    }

    // index function for F
    static uint64_t f(uint64_t n) {
      if (n == 0) {
        return 0;
      }
      uint64_t i = 1;
      uint64_t current = F(i);
      while (current < n) {
        i++;
        current = F(i);
      }

      if (current == n) {
        return i;
      } else {
        return i - 1;
      }
    }

    static void prepareTask(SignalGroup *group, uint64_t idx, uint64_t to) {
      auto *group_ = group;
      auto idx_ = idx;
      auto to_  = to;
      group->getTask(idx)->set([group_, idx_, to_](Task&) {
          ASSERT(group_ != nullptr); // TODO: handle case when group is not valid anymore
          ASSERT(to_ > 0);
          auto to_tmp = to_;
          while (true) {
          uint64_t n = to_tmp - idx_ + 1;
          if ( n < 2) {
          return;
          }
          uint64_t j = TreeStrategy::F(TreeStrategy::f(n) - 1);
          auto child_idx = j + idx_;
          //MLOG_ERROR(mlog::app, idx_, "sends to", j + from_);
          ISignalable* dest = group_->getMember(child_idx);
          if (dest) {
          if (child_idx < to_tmp) {
          TreeStrategy::prepareTask(group_, child_idx, to_tmp);
          dest->addTask(&group_->getTask(child_idx)->list_member);
          }
          dest->signal();
          }
          to_tmp = child_idx - 1;
          }
      });
    }

    static const uint64_t NARY = 3;
    static void sendTo(SignalGroup *group, uint64_t idx, uint64_t size) {
      ASSERT(idx < size);
      ASSERT(group != nullptr);
      for (uint64_t i = 0; i < NARY; i++) {
        auto child_idx = idx * NARY + i + 1;
        if (child_idx >= size) {
          break;
        }
        TreeStrategy::multicast(group, child_idx, size);
      }
    }

    static void multicast(SignalGroup *group, uint64_t idx, uint64_t size) {
      //MLOG_ERROR(mlog::app, "multicast", DVAR(from), DVAR(idx));
      ISignalable *dest = group->getMember(idx);

      if (idx <= (size-1) / NARY) {
        auto *t = group->getTask(idx);
        //while (not t->isUnused()) {}
        t->set([group, idx, size](Task&) { TreeStrategy::sendTo(group, idx, size); } );
        dest->addTask(&t->list_member);
      }
      dest->signal();
    }

    static void cast(SignalGroup *group, uint64_t idx, uint64_t size) {
      auto *signalable = group->getMember(idx);
      if (signalable) {
        auto *t = group->getTask(0);
        while (not t->isUnused()) {}
        //t->set([group, size](Task&) { sendTo(group, 0, size);  });
        //signalable->addTask(&group->getTask(0)->list_member);
        //signalable->signal();

        prepareTask(group, idx, size-1);
        signalable->addTask(&t->list_member);
        signalable->signal();
      }
    }
};
