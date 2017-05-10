## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## TODO
- [x] Makefile - Incluir coisas do dropboxUtil
- [x] Integrar exemplos socket
- [x] Ver como acoes vao ser passadas para o servidor e vice-versa
- [x] get_sync_dir
- [ ] list: funcionando, mas não para um conjunto muito grande de arquivos. como fazer ele encerrar apenas após ter baixado todos? (nem antes e nem muito tempo depois)
- [ ] upload
- [ ] download
- [ ] outras coisas
- [x] .gitignore
- [ ] Segurança: e se o usuário passar um filename com "../" vai baixar algum arquivo indevido?
