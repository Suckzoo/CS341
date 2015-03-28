echo "server must be opened!"
$regx := '^[0-9]+$'
if [[ "$1" =~ $regx ]] ; then
	./client -h 127.0.0.1 -p "$1" -m 1 < binary2.jpg > out1 &
	./client -h 127.0.0.1 -p "$1" -m 1 < binary3.jpg > out2 &
	./client -h 127.0.0.1 -p "$1" -m 1 < binary2.jpg > out3 &
	./client -h 127.0.0.1 -p "$1" -m 1 < binary3.jpg > out4 &
	echo "phase 1"
	ls -l out1 out2 out3 out4
	sleep 1
	echo "phase 2"
	ls -l out1 out2 out3 out4
	sleep 1
	echo "phase 3"
	ls -l out1 out2 out3 out4
	sleep 1
	echo "phase 4"
	ls -l out1 out2 out3 out4
	sleep 1
	echo "final phase"
	ls -l out1 out2 out3 out4
	sleep 1
else
	echo "Usage : ./multi-shot.sh [port]"
fi
