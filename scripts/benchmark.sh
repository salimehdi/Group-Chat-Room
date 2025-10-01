# --- Configuration ---
CLIENT_COUNT=100
MSG_RATE=20
DURATION=20 
RESULTS_FILE="results/benchmark_results.csv"
SERVER_BINARY="./build/low_latency_server"
CLIENT_SCRIPT="python_clients/bot.py"

# --- Build Step ---
echo "Building the server..."
mkdir -p build
cd build
cmake ..
make
cd ..
if [ ! -f "$SERVER_BINARY" ]; then
    echo "Build failed. Exiting."
    exit 1
fi
echo "Build successful."
echo ""


# --- Cleanup Function ---
cleanup() {
    echo "Cleaning up old processes and results..."
    pkill -f low_latency_server
    pkill -f "python3 ${CLIENT_SCRIPT}"
    rm -f /tmp/latency_result_*.txt
    sleep 1
}

# --- Data Aggregation Function ---
aggregate_results() {
    local test_name=$1
    
    wait
    sleep 1

    local total_messages=0
    local avg_latency=0
    
    if ls /tmp/latency_result_*.txt 1> /dev/null 2>&1; then
        avg_latency=$(cat /tmp/latency_result_*.txt | awk -F',' '{ sum += $1; n++ } END { if (n > 0) print sum / n; else print 0; }')
        p99_latency=$(cat /tmp/latency_result_*.txt | awk -F',' '{ sum += $3; n++ } END { if (n > 0) print sum / n; else print 0; }')
        
        throughput=$((CLIENT_COUNT * MSG_RATE))
    else
        avg_latency="N/A"
        p99_latency="N/A"
        throughput="N/A"
    fi
    
    echo "Results for ${test_name}:"
    echo "  - Average Latency (across clients): ${avg_latency} µs"
    echo "  - Average P99 Latency (across clients): ${p99_latency} µs"
    echo "  - Total Throughput: ${throughput} msgs/sec"
    
    echo "${test_name},${avg_latency},${p99_latency},${throughput}" >> ${RESULTS_FILE}
}

# --- Main Execution ---
mkdir -p results
echo "TestName,AvgLatency_us,AvgP99Latency_us,Throughput_msgs_sec" > ${RESULTS_FILE}

cleanup
echo "--- Starting Test 1: Epoll Server with ${CLIENT_COUNT} TCP Clients ---"
$SERVER_BINARY epoll &
SERVER_PID=$!
sleep 2 

# Start clients
for i in $(seq 1 $CLIENT_COUNT); do
  python3 $CLIENT_SCRIPT --mode tcp --id $i --rate $MSG_RATE --duration $DURATION --sender &
done
echo "Clients running for $DURATION seconds..."
sleep $DURATION
kill $SERVER_PID
aggregate_results "Epoll_TCP"


# === Test 2: Thread-per-Connection Server (TCP) ===
cleanup
THREAD_CLIENT_COUNT=25
echo ""
echo "--- Starting Test 2: Thread-per-Connection Server with ${THREAD_CLIENT_COUNT} TCP Clients ---"
$SERVER_BINARY thread &
SERVER_PID=$!
sleep 2

# Start clients
for i in $(seq 1 $THREAD_CLIENT_COUNT); do
  python3 $CLIENT_SCRIPT --mode tcp --id $i --rate $MSG_RATE --duration $DURATION --sender &
done
echo "Clients running for $DURATION seconds..."
sleep $DURATION
kill $SERVER_PID
CLIENT_COUNT=$THREAD_CLIENT_COUNT
aggregate_results "ThreadPerConn_TCP"
CLIENT_COUNT=100


# === Test 3: UDP Multicast (One-way Latency) ===
cleanup
echo ""
echo "--- Starting Test 3: Epoll Server with UDP Multicast ---"
$SERVER_BINARY epoll &
SERVER_PID=$!
sleep 2

# Start N-1 listeners and 1 sender
for i in $(seq 1 $(($CLIENT_COUNT - 1)) ); do
  python3 $CLIENT_SCRIPT --mode multicast --id $i &
done
python3 $CLIENT_SCRIPT --mode tcp --id $CLIENT_COUNT --rate $MSG_RATE --duration $DURATION --sender &

echo "Multicast test running for $DURATION seconds..."
sleep $DURATION
kill $SERVER_PID
# Kill listener
pkill -f "python3 ${CLIENT_SCRIPT} --mode multicast"
aggregate_results "Epoll_UDP_Multicast"

# --- Final Cleanup ---
cleanup
echo ""
echo "Results saved to ${RESULTS_FILE}"
cat ${RESULTS_FILE}