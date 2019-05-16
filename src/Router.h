#include <G3D/G3D.h>
#include "DistributedRenderer.h"
#include "ImageDist.h"
#include "TextureDist.h"

using namespace std;
using namespace DistributedRenderer;
using namespace G3D;

/* =========================================
 * =========================================
 *            Distributed Router
 * =========================================
 * =========================================
 * 
 * A Router built on G3D NetConnections to service
 * a remote rendering distributed network
 *
 * -----
 *
 * REGISTRATION:
 *
 * Upon starting, a server will listen for connections. The server
 * will listen indefinitely until a Client connects, at which point
 * the server will make a deadline for remote nodes to connect, after
 * which it will begin when it has at least one remote node. At this
 * point the router will ignore any incoming messages that it has not
 * already registered as a remote or client node.
 *
 *
 * CONFIG:
 * 
 * Given valid connections the router will calculate the screen fragments 
 * for each remote node and send a CONFIG packet with that info to each 
 * node respectively. It is up to the remote nodes to respond with a 
 * CONFIG_RECEIPT packet asserting that the nodes successfully started their 
 * applications with the received screen data. The router will 
 * tally the responses, and when all are accounted for, the router
 * signals the client to start by broadcasting a READY packet to 
 * the network, also signalling the remote nodes.
 *
 *
 * RUNNING:
 *
 * On reception of an UPDATE packet, the router will reroute
 * the packet to all remote nodes. If the current frame build
 * is not complete, the rotuer will flush it and reset because that
 * frame missed the deadline on the client by now.
 *
 * On reception of a FRAGMENT packet, the router will add it 
 * to the build buffer for the current frame. If this makes the 
 * build buffer is full, the router will send the finished frame
 * to the client as a PNG.
 *
 * Coming soon, dynamic rebalancing on node failure
 *
 *
 * TERMINATION:
 *
 * On reception of a TERMINATE packet from the client, the router
 * will broadcast the terminate packet and cut off all connections.
 *
 */

namespace DistributedRenderer{
	namespace Router{
		enum RouterState {
		    OFFLINE,
		    IDLE,
		    REGISTRATION,
		    CONFIGURATION,
		    LISTENING,
		    TERMINATED
		};

		typedef struct {
		    bool configured;
		    uint32 id;
		    uint32 y;
		    uint32 h;
		    int frag_loc;
		    shared_ptr<NetConnection> connection;
		} remote_connection_t;

		class Router{
			private:
				RouterState router_state;

				shared_ptr<NetServer> server;
				shared_ptr<NetConnection> client;

				uint32 current_batch;
				uint32 pieces;
				Array<shared_ptr<ImageDist>> fragments;

				// this registry will track remote connections, addressable with IP addresses
				map<uint32, remote_connection_t*> remote_connection_registry;

				// setup
				void addClient(shared_ptr<NetConnection> conn);
				void addRemote(shared_ptr<NetConnection> conn);
				void removeRemote(NetAddress& addr);

				void setState(RouterState s) { router_state = s; }

				// networking
				void broadcast(PacketType t, BinaryOutput* header, BinaryOutput* body, bool include_client);
				void broadcast(PacketType t, bool include_client);
				void send(PacketType t, shared_ptr<NetConnection> conn, BinaryOutput* header, BinaryOutput* body);
				void send(PacketType t, shared_ptr<NetConnection> conn);

				void fastsend(PacketType t, shared_ptr<NetConnection> conn);
				void fastsend(PacketType t, shared_ptr<NetConnection> conn, BinaryOutput* header, BinaryOutput* body);

				void registration();
				void configuration();

				// packet handlers
				void rerouteUpdate(BinaryInput* header, BinaryInput* body);
				void handleFragment(remote_connection_t* conn_vars, BinaryInput* header, BinaryInput* body);

			public:
				Router() : pieces(0), current_batch(1000), router_state(OFFLINE) {
					cout << "Router started up" << endl;
				}

				bool setup();
				void poll();
				void terminate();

				// accessors
				RouterState getState() { return router_state; }
				uint32 numRemotes() { return remote_connection_registry.size();}
		};
	}
}