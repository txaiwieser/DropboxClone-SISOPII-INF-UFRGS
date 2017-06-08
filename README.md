## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## TO DO
### bugs
- [ ] as vezes quando um arquivo é enviado para outro dispositivo, o arquivo do servidor fica com 0 bytes (ou as vezes o do cliente)
- [ ] quando cliente recebe PUSHs seguidos, fica trancado no while após o segundo ou terceiro(?)

### Alta prioridade
- [ ] Ao iniciar o cliente, os arquivos que foram modificados, deletados e adicionados ao sync_dir enquanto o cliente não estava online devem ser enviados ao servidor
- [ ] Onde usar semáforos e mutex? (1: impedir que um usuario baixe, em um dispositivo, um arquivo enquanto ele está sendo enviado ao servidor por outro dispositivo; 2: impedir que daemon rode enquanto o usuario está upando ou fazendo download?; 3: onde mais?)

### Média prioridade
- [ ] Tem como reutilizar mais código entre as funcoes de send e receive files? Modularizar melhor...

### Baixa prioridade / ideias extras
- [ ] Se a conexão com o servidor cair, o cliente poderia ser avisado
- [ ] Barra de progresso ao fazer download e upload?
- [ ] Segurança: se o usuário passar um filename com "../" vai baixar algum arquivo indevido? Pode até baixar arquivo de outra pessoa. (Atualmente isso ta gerando um segfault.. teria que tratar)
- [ ] Ver se o retorno do write e read foi completo, sempre? Alberto disse que tem uma chance remota de isso dar problema. Em localhost deve ser muito raro.
- [ ] Criar outro socket só p upload e download, como alberto falou na aula? nao entendi direito


## Revisar / Finalizar
- [ ] Código bem comentado, bons nomes de variáveis, identação...
- [ ] Checar se tamanhos de strings e tipos usados são suficientes e adequados (alguns lugares tá usando int32_t, mas melhor nao seria usar uint32_t?)
- [ ] Liberar memória, free() ...
- [ ] Mensagens de debug ajeitar...
- [ ] Confirmar que threads estão sendo fechadas ao encerrar execução. (Problema com porta deveria sumir em caso afirmativo, certo?)
- [ ] Testar e confirmar se o tratamento de erros está adequado
- [ ] Remover warnings
