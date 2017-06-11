## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## Alta prioridade
- [ ] Os arquivos devem ser sincronizados toda vez que o cliente conecta ou não? Seria uma boa pelo menos baixar os do servidor que estão com timestamp diferente...

## Baixa prioridade
- [ ] Transmitir timestamps como string? É a forma mais segura aparentemente

## Extra
- [ ] Ao iniciar o cliente, os arquivos que foram modificados, deletados e adicionados ao sync_dir enquanto o cliente não estava online devem ser enviados ao servidor

### Ideias (abandonadas no momento)
- [ ] Ver se o retorno do write e read foi completo, sempre? Alberto disse que tem uma chance remota de isso dar problema. Em localhost deve ser muito raro.
- [ ] Criar outro socket só p upload e download, como alberto falou na aula? nao entendi direito
- [ ] Barra de progresso ao fazer download e upload?
- [ ] Segurança: se o usuário passar um filename com "../" vai baixar algum arquivo indevido? Pode até baixar arquivo de outra pessoa. (Atualmente isso ta gerando um segfault.. teria que tratar)
- [ ] Tratamento de erros: funções send_file, get_file, receive_file (on server), send_file (on server). Se arquivo não pode ser aberto, deve retornar um erro e sair. Exibir mensagens de sucesso também?
