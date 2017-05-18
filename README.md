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
