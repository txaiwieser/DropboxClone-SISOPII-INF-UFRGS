## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## Alta prioridade
- [ ] Às vezes o get_file recebe o tamanho do arquivo errado, e aí fica num loop infinito. (Talvez isso aconteça com outras funções como send_file também, pois a troca de mensagens é similar). Tem uma possivel solucao em outro branch (mas ta dando um outro bug...)
- [ ] Os arquivos devem ser sincronizados toda vez que o cliente conecta ou não? Seria uma boa pelo menos baixar os do servidor que estão com timestamp diferente...


## Média prioridade
- [ ] Transmitir timestamps como string? É a forma mais segura aparentemente
- [ ] Ajeitar saída do programa. Há varios TODOs relacionados a isso..

## Revisar / Finalizar
- [ ] Testar muito bem para garantir que mutex estão corretos (movi o do inotify mas não cheguei a testar muito bem). Testar tratamento de erros. Testar interface com DEBUG=0, incluir mensagens de sucesso
- [ ] tirar prints de debug, warnings... desativar debug na compilação final.. TODOs

## Extra
- [ ] Ao iniciar o cliente, os arquivos que foram modificados, deletados e adicionados ao sync_dir enquanto o cliente não estava online devem ser enviados ao servidor

### Ideias (abandonadas no momento)
- [ ] Ver se o retorno do write e read foi completo, sempre? Alberto disse que tem uma chance remota de isso dar problema. Em localhost deve ser muito raro.
- [ ] Criar outro socket só p upload e download, como alberto falou na aula? nao entendi direito
- [ ] Barra de progresso ao fazer download e upload?
- [ ] Segurança: se o usuário passar um filename com "../" vai baixar algum arquivo indevido? Pode até baixar arquivo de outra pessoa. (Atualmente isso ta gerando um segfault.. teria que tratar)
- [ ] Tratamento de erros: funções send_file, get_file, receive_file (on server), send_file (on server). Se arquivo não pode ser aberto, deve retornar um erro e sair. Exibir mensagens de sucesso também?
