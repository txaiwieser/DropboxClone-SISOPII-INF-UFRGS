# makefile ESQUELETO
#
# OBRIGATÓRIO ter uma regra "all" para geração da biblioteca e de uma
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#  1. Cuidado com a regra "clean" para não apagar o "fila2.o"
#
# OBSERVAR que as variáveis de ambiente consideram que o Makefile está no diretótio "cthread"
#

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src


all: util client server

util: 
	gcc -c $(SRC_DIR)/dropboxUtil.c -Wall -o $(BIN_DIR)/dropboxUtil.o
	
client:
	gcc -c $(SRC_DIR)/dropboxClient.c -Wall -o $(BIN_DIR)/dropboxClient.o
	#ar crs $(LIB_DIR)/libcthread.a $(BIN_DIR)/support.o $(BIN_DIR)/cthread.o

server:
	gcc -c $(SRC_DIR)/dropboxServer.c -Wall -o $(BIN_DIR)/dropboxServer.o

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
