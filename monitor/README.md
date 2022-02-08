# BUILD

mkdir bin && cd bin
cmake .. && make

# RUN
cd bin && ./otc-monitor  or
./otc-monitor -c <configure file> like this
./otc-monitor -c ../conf/conf.json


