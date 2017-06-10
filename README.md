## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## Alta prioridade
- [ ] Data de modificação dos arquivos nem sempre tá sendo salva. (comportamento muito aleatório)
- [ ] Sincronizar ao conectar pela primeira vez

## Revisar / Finalizar
- [ ] Resolver TODOs
- [ ] testar renomeacao de arquivo no inotify
- [ ] Tem como reutilizar e modularizar o código melhor?
- [ ] Checar se tamanhos de strings e tipos usados são suficientes e adequados (alguns lugares tá usando int32_t, mas melhor nao seria usar uint32_t?)
- [ ] Liberar memória, free() ...
- [ ] Mensagens de debug: colocar "Dropbox>" no cliente?
- [ ] Testar e confirmar se o tratamento de erros está adequado
- [ ] Testar muito bem para garantir que mutex estão corretos (movi o do inotify mas não cheguei a testar muito bem)
- [ ] Desativar debug da compilação final

## Extra
- [ ] Ao iniciar o cliente, os arquivos que foram modificados, deletados e adicionados ao sync_dir enquanto o cliente não estava online devem ser enviados ao servidor


### Ideias (abandonadas no momento)
- [ ] Se a conexão com o servidor cair, o cliente poderia ser avisado
- [ ] Ver se o retorno do write e read foi completo, sempre? Alberto disse que tem uma chance remota de isso dar problema. Em localhost deve ser muito raro.
- [ ] Criar outro socket só p upload e download, como alberto falou na aula? nao entendi direito
- [ ] Barra de progresso ao fazer download e upload?
- [ ] Segurança: se o usuário passar um filename com "../" vai baixar algum arquivo indevido? Pode até baixar arquivo de outra pessoa. (Atualmente isso ta gerando um segfault.. teria que tratar)
