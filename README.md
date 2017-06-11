## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## Alta prioridade
- [ ] Data de modificação dos arquivos nem sempre tá sendo salva. (comportamento muito aleatório)
- [ ] Às vezes o get_file recebe o tamanho do arquivo errado, e aí fica num loop infinito. (Talvez isso aconteça com outras funções como send_file também, pois a troca de mensagens é similar)
- [ ] Resolver TODOs

## Revisar / Finalizar
- [ ] Testar muito bem para garantir que mutex estão corretos (movi o do inotify mas não cheguei a testar muito bem). Testar tratamento de erros. Testar interface com DEBUG=0.
- [ ] Desativar debug da compilação final
- [ ] tirar prints de debug, warnings...

## Extra
- [ ] Ao iniciar o cliente, os arquivos que foram modificados, deletados e adicionados ao sync_dir enquanto o cliente não estava online devem ser enviados ao servidor


### Ideias (abandonadas no momento)
- [ ] Se a conexão com o servidor cair, o cliente poderia ser avisado
- [ ] Ver se o retorno do write e read foi completo, sempre? Alberto disse que tem uma chance remota de isso dar problema. Em localhost deve ser muito raro.
- [ ] Criar outro socket só p upload e download, como alberto falou na aula? nao entendi direito
- [ ] Barra de progresso ao fazer download e upload?
- [ ] Segurança: se o usuário passar um filename com "../" vai baixar algum arquivo indevido? Pode até baixar arquivo de outra pessoa. (Atualmente isso ta gerando um segfault.. teria que tratar)
- [ ] Tratamento de erros: funções send_file, get_file, receive_file (on server), send_file (on server). Se arquivo não pode ser aberto, deve retornar um erro e sair. Exibir mensagens de sucesso também?
