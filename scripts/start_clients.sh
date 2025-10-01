if [ -z "$1" ]; then
  echo "Usage: $0 <num_clients> [duration_seconds]"
  exit 1
fi

NUM_CLIENTS=$1
DURATION=${2:-30}
RATE=10

echo "Starting $NUM_CLIENTS TCP clients for $DURATION seconds..."

# All clients will be both senders and receivers
for i in $(seq 1 $NUM_CLIENTS); do
  python3 python_clients/bot.py --mode tcp --id $i --rate $RATE --duration $DURATION --sender &
done

echo "Waiting for clients to finish..."
wait
echo "All clients finished."