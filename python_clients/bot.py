import socket
import threading
import time
import argparse
import struct
import sys
import statistics

# --- Configuration ---
SERVER_HOST = '127.0.0.1'
TCP_PORT = 8080
MCAST_GROUP = '239.0.0.1'
MCAST_PORT = 8081
BUFFER_SIZE = 1024

latencies = []

def get_monotonic_time_us():
    return time.monotonic_ns() // 1000

def tcp_listener(sock):
    """Listens for messages from the TCP server and calculates RTT."""
    while True:
        try:
            data = sock.recv(BUFFER_SIZE)
            if not data:
                break
            
            parts = data.decode('utf-8').split(':')
            if len(parts) >= 3:
                try:
                    sent_time_us = int(parts[2])
                    rtt_us = get_monotonic_time_us() - sent_time_us
                    latencies.append(rtt_us)
                except (ValueError, IndexError):
                    pass # Ignore wrong messages
        except ConnectionResetError:
            break
        except Exception as e:
            print(f"TCP listener error: {e}", file=sys.stderr)
            break
    print("TCP listener thread finished.")


def multicast_listener():
    """Listens for messages from the UDP multicast group."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        sock.bind(('', MCAST_PORT))
    except Exception as e:
        print(f"Error binding multicast socket: {e}", file=sys.stderr)
        return

    mreq = struct.pack("4sl", socket.inet_aton(MCAST_GROUP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    print(f"Subscribed to multicast group {MCAST_GROUP}:{MCAST_PORT}")

    while True:
        try:
            data, _ = sock.recvfrom(BUFFER_SIZE)
            parts = data.decode('utf-8').split(':')
            if len(parts) >= 3:
                try:
                    sent_time_us = int(parts[2])
                    one_way_latency_us = get_monotonic_time_us() - sent_time_us
                    latencies.append(one_way_latency_us)
                except (ValueError, IndexError):
                    pass
        except Exception as e:
            print(f"Multicast listener error: {e}", file=sys.stderr)
            break
    print("Multicast listener thread finished.")


def run_bot(bot_id, rate, duration, is_sender):
    """A bot that connects via TCP and sends messages at a given rate."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((SERVER_HOST, TCP_PORT))
    except ConnectionRefusedError:
        print(f"Bot {bot_id}: Connection refused.", file=sys.stderr)
        return

    listener = threading.Thread(target=tcp_listener, args=(sock,), daemon=True)
    listener.start()
    
    print(f"Bot {bot_id} connected and sending messages.")
    
    start_time = time.time()
    msg_count = 0
    
    while time.time() - start_time < duration:
        if is_sender:
            try:
                timestamp = get_monotonic_time_us()
                message = f"{bot_id}:{msg_count}:{timestamp}"
                sock.sendall(message.encode('utf-8'))
                msg_count += 1
                time.sleep(1.0 / rate)
            except (BrokenPipeError, ConnectionResetError):
                print(f"Bot {bot_id}: Server connection lost.", file=sys.stderr)
                break
            except Exception as e:
                print(f"Bot {bot_id} error: {e}", file=sys.stderr)
                break
        else:
            time.sleep(1)

    sock.close()


def main():
    parser = argparse.ArgumentParser(description="Chat System Client Bot")
    parser.add_argument("--mode", choices=['tcp', 'multicast'], required=True, help="Client mode")
    parser.add_argument("--id", type=int, default=0, help="Unique ID for this bot")
    parser.add_argument("--rate", type=int, default=10, help="Messages per second (for tcp mode)")
    parser.add_argument("--duration", type=int, default=30, help="How long to run the test in seconds")
    parser.add_argument("--sender", action='store_true', help="Make this bot a message sender (for tcp mode)")
    args = parser.parse_args()

    try:
        if args.mode == 'tcp':
            run_bot(args.id, args.rate, args.duration, args.sender)
        elif args.mode == 'multicast':
            multicast_listener()
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        if latencies:
            avg_latency = statistics.mean(latencies)
            median_latency = statistics.median(latencies)
            p99_latency = statistics.quantiles(latencies, n=100)[98] 
            
            with open(f"/tmp/latency_result_{args.id}.txt", "w") as f:
                f.write(f"{avg_latency:.2f},{median_latency:.2f},{p99_latency:.2f}\n")
            
            print(f"\n--- Bot {args.id} Stats ---")
            print(f"Messages recorded: {len(latencies)}")
            print(f"Avg Latency: {avg_latency:.2f} µs")
            print(f"Median Latency: {median_latency:.2f} µs")
            print(f"P99 Latency: {p99_latency:.2f} µs")


if __name__ == "__main__":
    main()