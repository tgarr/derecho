#pragma once

#include "detail/connection_manager.hpp"
#include "detail/p2p_connection_manager.hpp"
#include "group.hpp"
#include "view.hpp"

#include <derecho/conf/conf.hpp>
namespace derecho {

template <typename T>
class ExternalClientCaller {
private:
    /** The ID of this node */
    const node_id_t node_id;
    /** The internally-generated subgroup ID of the subgroup that this ExternalClientCaller will contact. */
    subgroup_id_t subgroup_id;
    /** The actual implementation of ExternalCaller, which has lots of ugly template parameters */
    std::unique_ptr<rpc::RemoteInvokerFor<T>> wrapped_this;
    ExternalGroup& group;

public:
    ExternalClientCaller(uint32_t type_id, node_id_t nid, subgroup_id_t subgroup_id, ExternalGroup& group);

    ExternalClientCaller(ExternalCaller&&) = default;
    ExternalClientCaller(const ExternalCaller&) = delete;

    template <rpc::FunctionTag tag, typename... Args>
    auto p2p_send(node_id_t dest_node, Args&&... args);
};

template <typename... ReplicatedTypes>
class ExternalGroup {
private:
    const node_id_t my_id;
    std::unique_ptr<View> prev_view;
    std::unique_ptr<View> curr_view;
    std::unique_ptr<tcp::tcp_connections> tcp_sockets;
    std::unique_ptr<sst::P2PConnectionManager> p2p_connections;
    std::mutex p2p_connections_mutex;
    std::unique_ptr<std::map<Opcode, receive_fun_t>> receivers;
    std::map<subgroup_id_t, std::list<PendingBase_ref>> fulfilled_pending_results;
    std::map<subgroup_id_t, uint64_t> max_payload_sizes;

    template <typename T>
    using external_caller_index_map = std::map<uint32_t, ExternalClientCaller<T>>;
    mutils::KindMap<external_caller_index_map, ReplicatedTypes...> external_callers;

    uint32_t get_index_of_type(const std::type_info& ti);

    /** ======================== copy/paste for rpc_manager ======================== **/
    std::atomic<bool> thread_shutdown{false};
    std::thread rpc_thread;
    /** p2p send and queries are queued in fifo worker */
    std::thread fifo_worker_thread;
    struct fifo_req {
        node_id_t sender_id;
        char* msg_buf;
        uint32_t buffer_size;
        fifo_req() : sender_id(0),
                     msg_buf(nullptr),
                     buffer_size(0) {}
        fifo_req(node_id_t _sender_id,
                 char* _msg_buf,
                 uint32_t _buffer_size) : sender_id(_sender_id),
                                          msg_buf(_msg_buf),
                                          buffer_size(_buffer_size) {}
    };
    std::queue<fifo_req> fifo_queue;
    std::mutex fifo_queue_mutex;
    std::condition_variable fifo_queue_cv;
    void p2p_receive_loop();
    void fifo_worker();
    void p2p_message_handler(node_id_t sender_id, char* msg_buf, uint32_t buffer_size);
    std::exception_ptr receive_message(const Opcode& indx, const node_id_t& received_from,
                                       char const* const buf, std::size_t payload_size,
                                       const std::function<char*(int)>& out_alloc);
    /** ======================== copy/paste for rpc_manager ======================== **/

public:
    ExternalGroup();
    ~ExternalGroup();

    template <typename SubgroupType>
    ExternalClientCaller<SubgroupType>& get_ref(uint32_t subgroup_index = 0);

    bool update_view();
    std::vector<node_id_t> get_members();
    std::vector<node_id_t> get_shard_members(uint32_t subgroup_id, uint32_t shard_num);
    template <typename SubgroupType>
    std::vector<node_id_t> get_shard_members(uint32_t subgroup_index, uint32_t shard_num);
};
}  // namespace derecho
