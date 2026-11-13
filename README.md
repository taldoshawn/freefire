# NOX-FF / ARM64

Build automatizado de um módulo visual Android (Java + JNI + C++20).

O repositório contém código de **renderização ESP** e uma API de entrada para câmera e entidades, mas **não contém um leitor funcional dos dados do Free Fire**. A instalação do módulo não torna o ESP operacional em partidas.

## Rodar pelo celular

Abra **Actions → NOX ARM64 → Run workflow**, e selecione:

- `module`: compila `libnox_esp.so` e `classes.dex` e publica como artefato.
- `apk`: baixa `original.apk` de uma Release privada indicada por tag, insere o módulo e assina um APK de teste.

O modo `apk` exige um APK original **completo** na Release, no mesmo repositório. Os ZIPs parciais não substituem esse APK.

### Arquivos

- `android/java/com/nox/esp/`: overlay e inicialização via ContentProvider.
- `android/native/`: JNI e contrato para receber quadros de entidades/câmera.
- `tools/patch_apk.py`: inclusão do módulo num APK decodificado.
- `.github/workflows/build.yml`: compilação no GitHub Actions.

### Limitações

O artefato ARM64 pode ser compilado sem fontes do jogo. Sem um adaptador que envie dados reais para `nox_publish_frame`, o overlay não tem jogadores para desenhar.

Reassinar o pacote altera a assinatura; serviços do jogo podem rejeitar o APK ou a atualização sobre a instalação oficial.
