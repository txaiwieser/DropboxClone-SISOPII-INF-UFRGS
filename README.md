## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## TO DO
- [x] Makefile - Incluir coisas do dropboxUtil
- [x] Integrar exemplos socket
- [x] Ver como acoes vao ser passadas para o servidor e vice-versa
- [x] get_sync_dir
- [x] list
- [x] upload
- [x] download
- [x] .gitignore
- [ ] Semaforos, mutex... onde?
- [ ] mutex 1: impedir que um usuario baixe, em um dispositivo, um arquivo enquanto ele está sendo enviado ao servidor por outro dispositivo.
- [ ] mutex 2 : impedir que daemon rode enquanto o usuario está upando ou fazendo download?
- [ ] verificar especificação
- [ ] Confirmar (com ps aux) se threads estão realmente sendo fechadas. Problema com porta acontecendo ainda?
- [ ] Teste e tratamento de erros
- [ ] checar se os valores máximos das strings e os tipos (int) são suficientes?
- [ ] Segurança: e se o usuário passar um filename com "../" vai baixar algum arquivo indevido?
- [ ] Renomear coisas do tailq? melhorar comentarios, identar codigo
- [ ] Liberar memória, free() ...
- [ ] Remover warnings
- [ ] Tem como reutilizar mais código entre as funcoes de send e receive files?
- [ ] barra de progresso ao fazer download e upload?


## Dúvidas para pedir para o monitor
- Se o arquivo foi deletado da pasta do usuário, o daemon precisa fazer algo (apagar do servidor)?
- Qual deve ser o valor da MAX_FILES e MAX_NAME?
- sync_client() precisa ser implementada ou é o próprio daemon? Pode mudar parametros dela (o pthread exige)?
- sync_server() porque o servidor iria precisar se atualizar com o diretorio do usuário? se o servidor chamar essa funcao, o cliente terá que ouvi-la. logo, seria necessário mais uma thread no cliente para ficar recebendo as requisicoes de sync do servidor. Se tiver 2 clientes com conteudos diferentes na sua pasta, o servidor iria sincronizar com qual das duas?
- Qual seria o melhor valor pro listen? A gente colocou 3
