#pragma once


#include "objects/ISignalable.hh"
#include "objects/ExecutionContext.hh"
#include "objects/CapRef.hh"
#include "objects/mlog.hh"
#include "util/Time.hh"


namespace mythos {

extern SleepEmulator emu;

extern Timer taskletTimer[250];
extern Timer wakeupTimer[250];

extern uint64_t taskletValues[250];
extern uint64_t wakeupValues[250];

/**
 * Strategy implementation, which constructs a "fibonacci" multicast tree of the group members.
 * Tree roughly looks like:
 * The first nodes receiving its Signal have the most time to distribute it further.
 * Therefore they get a bigger range of nodes to transfer the Signal to.
 * Treeconstruction is dependent on the LATENCY parameter, which has to be adapted for different hardware.
 */
struct TreeCastStrategy {
    // Latency: inverse ratio of sending overhead and complete transfer time
    // LATENCY = 2 optimal on KNC if no deep sleep
    // higher latency better if threads are in deep sleep
    // could check everything for deep sleep and choose latency dynamically?
    // could skip deep sleep nodes and signal children from own context ->ok
    static const uint64_t LATENCY = 2;
    static const uint64_t RECURSIVE_SIZE = 25;
    static int64_t tmp[RECURSIVE_SIZE]; //recursive memory

    // Helper functions to calculate fibonacci tree for optimal multicast
    // saves result of previous calcs to optimize expensive recursion
    static size_t F(size_t time) {
        if (time < LATENCY) {
            return 1;
        }
        if (time < 20 && tmp[time] > 0) {
            return tmp[time];
        }
        auto ret = F(time - 1) + F(time - LATENCY);
        if (time < 20) {
            tmp[time] = ret;
        }
        return ret;
    }

    // index function for F
    static uint64_t f(uint64_t n) {
        Timer t;
        t.start();
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

    static uint64_t getSleepState(HWThread *hwt) {
        if (!hwt) return 0;
        auto apicID = hwt->getApicID();
        auto sleepState = emu.getSleepState(apicID);
        return sleepState;
    }

    static void signalTo(SignalGroup *group, uint64_t idx, uint64_t from, uint64_t to) {
        auto to_tmp = to;
        // Signal own EC, should be ready when leaving kernel
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        while (true) {
            uint64_t n = to_tmp - from + 1;
            if ( n < 2) {
                break;
            }
            // calculates the split of the range depending on LATENCY
            // left range is handled by this, right by other node
            uint64_t j = TreeCastStrategy::F(TreeCastStrategy::f(n) - 1);
            auto destID = j + from;
            if (destID < to_tmp) {
                multicast(group, destID, destID, to_tmp);
            } else { // if leaf node, which does not forward, just signal it
                TypedCap<ISignalable> dest(group->getMember(destID)->cap());
                dest->signal(0);
            }
            to_tmp = destID - 1;
        }
        own->signal(0);
    }

    static void multicast(SignalGroup *group, uint64_t idx, uint64_t from, uint64_t to) {
        auto *t = group->getTasklet(idx);
        while (not t->isUnused()) {
          MLOG_ERROR(mlog::boot, "Tasklet in use!!");
          mythos::hwthread_pause(300);
        }
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        if (own && getSleepState(own->getHWThread()) < 2) { // child not in deep sleep send Tasklet
            t->set([group, idx, from, to](TransparentTasklet*) {
                signalTo(group, idx, from, to);
            });
            auto home = own->getHWThread()->getHome();
            home->run(t);
        } else { // Send to child yourself because is in deep sleep or own not valid anymore
            signalTo(group, idx, from, to);
        }
    }

};


/**
 * N-Ary tree for comparison with the Fibonacci Tree approach
 */
struct NaryTree {
    static const uint64_t N = 3;

    static uint64_t getSleepState(HWThread *hwt) {
        if (!hwt) return 0;
        auto apicID = hwt->getApicID();
        auto sleepState = emu.getSleepState(apicID);
        return sleepState;
    }

    static void sendTo(SignalGroup *group, uint64_t idx, uint64_t size) {
        MLOG_DETAIL(mlog::boot, DVAR(group), DVAR(idx), DVAR(size));
        ASSERT(idx < size);
        ASSERT(group != nullptr); // TODO: parallel deletion of group?
        TypedCap<ISignalable> own(group->getMember(idx)->cap());
        ASSERT(own);

        //uint64_t values[N] {0};
        //uint64_t tasklets[N] {0};
        //uint64_t wakeups[N] {0};
        //mythos::Timer t;
        for (uint64_t i = 0; i < N; i++) {
            //t.start();
            auto child_idx = idx * N + i + 1;
            if (child_idx >= size) {
                break;
            }
            NaryTree::multicast(group, child_idx, size);

            //values[i] = t.end();
            //tasklets[i] = taskletValues[idx+4];
            //wakeups[i] = wakeupValues[idx+4];
        }
        // Signal own EC, will be scheduled after kernel task handling
        own->signal(0);
        //for (auto i = 0ul; i < N && idx*N+i+1 < size; i++)
        //   MLOG_ERROR(mlog::boot, idx +4,idx*N+i+5, values[i]);

/*
        for (auto i = 0ul; i < N; i++) {
          if (idx*N+i+1 < size)
            MLOG_ERROR(mlog::boot, idx + 4, idx * N + i + 5, tasklets[i], values[i], wakeups[i]);
        }
*/

    }

    static void multicast(SignalGroup *group, uint64_t idx, uint64_t size) {
      auto *t = group->getTasklet(idx); // 50 cycles

      while (not t->isFree()) { // ~ 2000 Cycles for whole 235 gorup cast
        MLOG_ERROR(mlog::boot, "Tasklet in use!!");
        mythos::hwthread_pause(200); // to reduce cache contention if in use
      }

      TypedCap<ISignalable> own(group->getMember(idx)->cap()); // 500 - 1000 cycles

      if (own && idx <= (size-1)/N /* && getSleepState(own->getHWThread()) < 2*/) {
          //MLOG_ERROR(mlog::boot, DVAR(group), DVAR(idx), DVAR(size), DVAR(sleepState));
          auto *home = own->getHWThread()->getHome(); //800 - 1600 cycles and up to 2000
          t->set([group, idx, size](TransparentTasklet*) {
              NaryTree::sendTo(group, idx, size);
          });
          //wakeupTimer[id].start();
          home->run(t); // 500 -1000
          //wakeupValues[id] = wakeupTimer[id].end();
      } else {
          NaryTree::sendTo(group, idx, size);
      }
    }
};

class TreeMulticast
{
public:
    static Error multicast(SignalGroup *group, size_t groupSize) {
        ASSERT(group != nullptr);
        TreeCastStrategy::multicast(group, 0, 0, groupSize-1);
        //NaryTree::multicast(group, 0, groupSize);
        return Error::SUCCESS;
    }
};

} // namespace mythos
