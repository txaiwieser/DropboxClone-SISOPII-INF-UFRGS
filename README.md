## Utilização
1. Compile ```make```
2. Inicie o servidor: ```./bin/dropboxServer 3003```
3. Inicie o cliente: ```./bin/dropboxClient usuario 127.0.0.1 3003```

## TO DO
### bugs
- [ ] sleep nao parece estar de fato esperando 3 segundos?
- [ ] as vezes quando um arquivo é enviado para outro dispositivo, o arquivo do servidor fica com 0 bytes (ou as vezes o do cliente)
- [ ] quando cliente recebe PUSHs seguidos, fica trancado no while após o segundo ou terceiro(?)
- [ ] Ao apagar o arquivo de um cliente, é encaminhado o DELETE ao servidor, que deleta o arquivo e depois manda todos outros dispositivos conectar. Isso está funcioonando, o problema é que o inotify do cliente no momento está apenas ignorando os arquivos que foram recentemente baixados do servidor, e nao quando são deletados. Assim, se o cliente deleta um arquivo, o servidor também deleta e manda o segundo dispositivo deletar. o arquivo é apagado do segundo dispositivo, mas aí o inotify acha que foi removido 'voluntariamente' pelo usuário, e entao manda o servidor deletar o arquivo tb. O servidor então exibe uma msg de File not found, pois já não existe arquivo com esse nome nele. Aparentemente isso nao tem gerado problemas, mas o ideal é arrumra isso pra nao enviar a request pro servidor. (POtencialmente podem ocorrer problemas se o primeiro cliente apagar o arquivo e em seguida recriá-lo, antes que o inotify do segundo cliente acorde do sleep)

### Alta prioridade
- [ ] Ao iniciar o cliente, os arquivos que foram modificados, deletados e adicionados ao sync_dir enquanto o cliente não estava online devem ser enviados ao servidor
- [ ] Onde usar semáforos e mutex? (1: impedir que um usuario baixe, em um dispositivo, um arquivo enquanto ele está sendo enviado ao servidor por outro dispositivo; 2: impedir que daemon rode enquanto o usuario está upando ou fazendo download?; 3: onde mais?)

### Média prioridade
- [ ] Se a conexão com o servidor cair, o cliente poderia ser avisado
- [ ] Segurança: se o usuário passar um filename com "../" vai baixar algum arquivo indevido? Pode até baixar arquivo de outra pessoa. (Atualmente isso ta gerando um segfault.. teria que tratar)
- [ ] Tem como reutilizar mais código entre as funcoes de send e receive files?
- [ ] Ver se o retorno do write e read foi completo, sempre? Alberto disse que tem uma chance remota de isso dar problema. Em localhost deve ser muito raro.
- [ ] Criar outro socket só p upload e download, como alberto falou na aula? nao entendi direito

### Baixa prioridade / ideias extras
- [ ] Barra de progresso ao fazer download e upload?

## Revisar / Finalizar
- [ ] Código bem comentado, bons nomes de variáveis, identação...
- [ ] Checar se tamanhos de strings e tipos usados são suficientes e adequados (alguns lugares tá usando int32_t, mas melhor nao seria usar uint32_t?)
- [ ] Liberar memória, free() ...
- [ ] Mensagens de debug ajeitar...
- [ ] Confirmar que threads estão sendo fechadas ao encerrar execução. (Problema com porta deveria sumir em caso afirmativo, certo?)
- [ ] Testar e confirmar se o tratamento de erros está adequado
- [ ] Remover warnings

## Relatório
- mudanças feitas nas structs do server
- dificuldades enfrentadas
