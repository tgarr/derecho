/*
 * This test measures the bandwidth of Derecho raw (uncooked) sends in GB/s as a function of
 * 1. the number of nodes 2. the number of senders (all sending, half nodes sending, one sending)
 * 3. message size 4. window size 5. number of messages sent per sender
 * 6. delivery mode (atomic multicast or unordered)
 * The test waits for every node to join and then each sender starts sending messages continuously
 * in the only subgroup that consists of all the nodes
 * Upon completion, the results are appended to file data_derecho_bw on the leader
 */
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <time.h>
#include <vector>

#include "aggregate_bandwidth.h"
#include "derecho/derecho.h"
#include "log_results.h"

using std::cout;
using std::endl;
using std::map;
using std::vector;

using namespace derecho;

struct exp_result {
    uint32_t num_nodes;
    uint num_senders_selector;
    long long unsigned int max_msg_size;
    unsigned int window_size;
    uint num_messages;
    uint delivery_mode;
    double bw;

    void print(std::ofstream& fout) {
        fout << num_nodes << " " << num_senders_selector << " "
             << max_msg_size << " " << window_size << " "
             << num_messages << " " << delivery_mode << " "
             << bw << endl;
    }
};

int main(int argc, char* argv[]) {
    if(argc < 5) {
        cout << "Insufficient number of command line arguments" << endl;
        cout << "Enter num_nodes, num_senders_selector (0 - all senders, 1 - half senders, 2 - one sender), num_messages, delivery_mode (0 - ordered mode, 1 - unordered mode)" << endl;
        cout << "Thank you" << endl;
        return -1;
    }
    pthread_setname_np(pthread_self(), "bw_test");

    // initialize the special arguments for this test
    const uint num_nodes = std::stoi(argv[1]);
    const uint num_senders_selector = std::stoi(argv[2]);
    const uint num_messages = std::stoi(argv[3]);
    const uint delivery_mode = std::stoi(argv[4]);

    // Read configurations from the command line options as well as the default config file
    Conf::initialize(argc, argv);

    // variable 'done' tracks the end of the test
    volatile bool done = false;
    // callback into the application code at each message delivery
    auto stability_callback = [&num_messages,
                               &done,
                               &num_nodes,
                               num_senders_selector,
                               num_delivered = 0u](uint32_t subgroup, int sender_id, long long int index, char* buf, long long int msg_size) mutable {
        // increment the total number of messages delivered
        ++num_delivered;
        if(num_senders_selector == 0) {
            if(num_delivered == num_messages * num_nodes) {
                done = true;
            }
        } else if(num_senders_selector == 1) {
            if(num_delivered == num_messages * (num_nodes / 2)) {
                done = true;
            }
        } else {
            if(num_delivered == num_messages) {
                done = true;
            }
        }
    };

    Mode mode = Mode::ORDERED;
    if(delivery_mode) {
        mode = Mode::UNORDERED;
    }

    auto membership_function = [num_senders_selector, mode, num_nodes](const View& curr_view, int& next_unassigned_rank) {
        subgroup_shard_layout_t subgroup_vector(1);
        auto num_members = curr_view.members.size();
        // wait for all nodes to join the group
        if(num_members < num_nodes) {
            throw subgroup_provisioning_exception();
        }
        // all senders case
        if(num_senders_selector == 0) {
            // a call to make_subview without the sender information
            // defaults to all members sending
            subgroup_vector[0].emplace_back(curr_view.make_subview(curr_view.members, mode));
        } else {
            // configure the number of senders
            vector<int> is_sender(num_members, 1);
            // half senders case
            if(num_senders_selector == 1) {
                // mark members ranked 0 to num_members/2 as non-senders
                for(uint i = 0; i <= (num_members - 1) / 2; ++i) {
                    is_sender[i] = 0;
                }
            } else {
                // mark all members except the last ranked one as non-senders
                for(uint i = 0; i < num_members - 1; ++i) {
                    is_sender[i] = 0;
                }
            }
            // provide the sender information in a call to make_subview
            subgroup_vector[0].emplace_back(curr_view.make_subview(curr_view.members, mode, is_sender));
        }
        next_unassigned_rank = curr_view.members.size();
        return subgroup_vector;
    };

    // Create just one subgroup of type RawObject
    map<std::type_index, shard_view_generator_t> subgroup_map = {{std::type_index(typeid(RawObject)), membership_function}};
    SubgroupInfo one_raw_group(subgroup_map);

    // join the group
    Group<> group(CallbackSet{stability_callback},
                  one_raw_group);

    cout << "Finished constructing/joining Group" << endl;
    // figure out the node's rank in the group to decide if it needs to send messages or not
    uint32_t node_id = getConfUInt32(CONF_DERECHO_LOCAL_ID);
    uint32_t node_rank = -1;
    auto members_order = group.get_members();
    cout << "The order of members is :" << endl;
    for(uint i = 0; i < num_nodes; ++i) {
        cout << members_order[i] << " ";
        if(members_order[i] == node_id) {
            node_rank = i;
        }
    }
    cout << endl;

    long long unsigned int max_msg_size = getConfUInt64(CONF_DERECHO_MAX_PAYLOAD_SIZE);

    // this function sends all the messages
    auto send_all = [&]() {
        RawSubgroup& raw_subgroup = group.get_subgroup<RawObject>();
        for(uint i = 0; i < num_messages; ++i) {
            // the lambda function writes the message contents into the provided memory buffer
            // in this case, we do not touch the memory region
            raw_subgroup.send(max_msg_size, [](char* buf) {});
        }
    };

    struct timespec start_time;
    // start timer
    clock_gettime(CLOCK_REALTIME, &start_time);
    // send all messages or skip if not a sender
    if(num_senders_selector == 0) {
        send_all();
    } else if(num_senders_selector == 1) {
        if(node_rank > (num_nodes - 1) / 2) {
            send_all();
        }
    } else {
        if(node_rank == num_nodes - 1) {
            send_all();
        }
    }
    // wait for the test to finish
    while(!done) {
    }
    // end timer
    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    long long int nanoseconds_elapsed = (end_time.tv_sec - start_time.tv_sec) * (long long int)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
    // calculate bandwidth measured locally
    double bw;
    if(num_senders_selector == 0) {
        bw = (max_msg_size * num_messages * num_nodes + 0.0) / nanoseconds_elapsed;
    } else if(num_senders_selector == 1) {
        bw = (max_msg_size * num_messages * (num_nodes / 2) + 0.0) / nanoseconds_elapsed;
    } else {
        bw = (max_msg_size * num_messages + 0.0) / nanoseconds_elapsed;
    }
    // aggregate bandwidth from all nodes
    double avg_bw = aggregate_bandwidth(members_order, node_id, bw);
    // log the result at the leader node
    if(node_rank == 0) {
        log_results(exp_result{num_nodes, num_senders_selector, max_msg_size,
                               getConfUInt32(CONF_DERECHO_WINDOW_SIZE), num_messages,
                               delivery_mode, avg_bw},
                    "data_derecho_bw");
    }

    group.barrier_sync();
    group.leave();
}