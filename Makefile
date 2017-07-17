CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src


all: dropboxUtil.o client server frontend

dropboxUtil.o:
	$(CC) -c $(SRC_DIR)/dropboxUtil.c -Wall -o $(BIN_DIR)/dropboxUtil.o

client: dropboxUtil.o
	$(CC) -o $(BIN_DIR)/dropboxClient $(SRC_DIR)/dropboxClient.c $(BIN_DIR)/dropboxUtil.o -Wall -pthread -lssl -lcrypto

server: dropboxUtil.o
	$(CC) -o $(BIN_DIR)/dropboxServer $(SRC_DIR)/dropboxServer.c $(BIN_DIR)/dropboxUtil.o -Wall -pthread -lssl -lcrypto

frontend: dropboxUtil.o
	$(CC) -o $(BIN_DIR)/dropboxFrontEnd $(SRC_DIR)/dropboxFrontEnd.c $(BIN_DIR)/dropboxUtil.o -Wall -pthread -lssl -lcrypto

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
