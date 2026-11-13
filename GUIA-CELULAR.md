# NOX-FF: usar GitHub Actions só com o celular

O repositório já contém o módulo visual e o workflow. Não exige PC para *compilar*. Não transforma o módulo em leitor de entidades do Free Fire.

## Gerar módulo ARM64

1. Abra o repositório privado no navegador do celular.
2. Toque em **Actions → NOX ARM64 → Run workflow** (use "Site para computador" caso o botão não apareça).
3. Selecione **module** e execute.
4. Quando a execução estiver verde, abra a execução e baixe **NOX-ARM64-module** em **Artifacts**.
5. O artefato contém \`libnox_esp.so\`, \`classes.dex\` e \`nox_api.h\`.

## Tentar montar APK modificado (somente com APK inteiro)

1. Abra **Releases → Draft a new release** no repositório privado.
2. Crie a tag \`inputs\`.
3. Anexe o APK original **completo** com o nome exato \`original.apk\`; ZIPs parciais não servem.
4. Publique a Release (mantendo o repositório privado).
5. Vá em **Actions → NOX ARM64 → Run workflow**; escolha \`apk\`, \`release_tag=inputs\`.
6. Se todos os passos terminarem, baixe \`NOX-ARM64-test-APK\` em Artifacts.

O workflow baixa o APK da Release, compila JNI e Java/DEX, usa apktool para incluir um ContentProvider de inicialização e adiciona \`libnox_esp.so\` em \`lib/arm64-v8a/\`. Depois faz zipalign e assina com chave de teste nova a cada execução.

**Limitação técnica:** o módulo usa \`nox_publish_frame\` como entrada para câmera e jogadores reais. Nenhum coletor de entidades/ossos específico da build foi desenvolvido ou testado. Sem ele, aparece apenas a mensagem de que o módulo aguarda dados.

## Se a execução falhar antes de qualquer etapa

Se a execução falhar com **nenhum runner atribuído** e **zero steps**, o código nem começou a compilar. Abra a página da execução e confira a mensagem do GitHub sobre acesso a runner, Actions e faturamento/limites de minutos. Isso não é erro de C++/Java, e não é solucionado alterando os arquivos-fonte.

## Problemas possíveis no APK

- O APK precisa ser completo e compatível com a versão esperada; \`ff.zip\`, \`ff2.zip\` e \`ffres.zip\` não formam um instalador pronto.
- Reconstruir com apktool pode falhar por recursos protegidos ou não decodificáveis.
- A nova assinatura não é a assinatura original: atualização sobre o aplicativo já instalado pode ser recusada, assim como verificações de integridade do jogo.
- O APK nunca foi instalado ou executado num Android nesta sessão.
