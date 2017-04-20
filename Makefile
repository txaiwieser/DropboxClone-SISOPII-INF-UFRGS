CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src


all: util client server

util: 
	gcc -c $(SRC_DIR)/dropboxUtil.c -Wall -o $(BIN_DIR)/dropboxUtil.o
	
client:
	gcc $(SRC_DIR)/dropboxClient.c -o $(BIN_DIR)/dropboxClient

server:
	gcc $(SRC_DIR)/dropboxServer.c -o $(BIN_DIR)/dropboxServer

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
