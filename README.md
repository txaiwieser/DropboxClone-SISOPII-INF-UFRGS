## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## TODO
- [x] Makefile - Incluir coisas do dropboxUtil
- [x] Integrar exemplos socket
- [x] Ver como acoes vao ser passadas para o servidor e vice-versa
- [x] get_sync_dir
- [x] list
- [x] upload
- [x] download
- [x] .gitignore
- [ ] verificar especificação
- [ ] Confirmar se processos filhos estão sendo fechados. Problema com porta acontecendo ainda?
- [ ] Teste e tratamento de erros
- [ ] TODO checar se os valores máximos das strings e os tipos (int) são suficientes?
- [ ] Segurança: e se o usuário passar um filename com "../" vai baixar algum arquivo indevido?
