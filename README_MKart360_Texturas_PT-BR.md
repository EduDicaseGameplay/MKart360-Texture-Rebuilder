# MKart360 — Guia de preparação, edição e reconstrução de texturas

Este guia foi escrito para acompanhar o projeto e explicar o fluxo que uso para preparar uma ROM própria de Mario Kart 64, extrair as texturas, editar cada categoria respeitando a estrutura original do jogo e reconstruir a ROM.

O objetivo não é simplesmente aumentar todas as imagens. Cada textura tem um formato, uma função e limites impostos pela ROM original. É preciso respeitar dimensões, transparência, paletas, áreas úteis, margens e a forma como o jogo carrega cada recurso.

## Estado atual do projeto

Na contagem de trabalho que uso como referência, **3.983 de 4.061 texturas já funcionam** seguindo as regras e limitações da estrutura da ROM. Restam **78 texturas sem uma solução confirmada**.

Esses números descrevem o estado atual do projeto, não uma garantia de que todas as texturas funcionarão em qualquer ROM ou revisão. A ROM de origem, seus offsets, formatos, compressões, paletas e alocações podem mudar. Por isso, mantenho sempre uma cópia intacta da ROM original e testo cada reconstrução.

## 1. O que preciso antes de começar

- Uma cópia legal da ROM de Mario Kart 64 que será usada como base.
- O projeto MKart360 completo, extraído para uma pasta local.
- Windows e PowerShell.
- Python 3.
- Pillow para leitura e escrita de PNG.
- Visual Studio 2010 e as ferramentas Xbox 360 XDK/SDK exigidas para compilar o port para Xbox 360. A reconstrução de texturas/ROM é uma etapa separada da compilação do executável.

> **Importante sobre a ROM:** o nome `baserom.br.z64` não transforma uma ROM em brasileira. O nome é apenas uma convenção usada pelo fluxo de trabalho. Se a ROM tiver outro nome ou região, informe o caminho e a região corretos. Não misture manifests extraídos de uma ROM com outra ROM sem validar offsets e metadados.

## 2. Instalação inicial

Abra o PowerShell na pasta `mk64-master`.

### 2.1 Instalar Python

Só preciso fazer isso uma vez:

```powershell
winget install Python.Python.3
```

Feche e abra o PowerShell novamente e confirme:

```powershell
py --version
```

Deve aparecer uma versão de Python 3.

### 2.2 Instalar Pillow

```powershell
py -m pip install --upgrade pip
py -m pip install pillow
```

### 2.3 Preparar as ferramentas de assets do Windows

Execute:

```powershell
powershell -ExecutionPolicy Bypass -File ".\xbox360\setup_windows_asset_tools.ps1" -SkipTorch
```

Essa etapa prepara as ferramentas previstas pelo projeto para trabalhar com os assets no Windows. Se o script informar uma dependência ausente, resolvo essa dependência antes de continuar; não ignoro erros de preparação.

## 3. Colocar e identificar a ROM

Copie a ROM que vou usar para a pasta:

```text
MKart360-Texture-Rebuilder\mk64-master\
```

Para a base brasileira usada neste fluxo, o nome convencional é:

```text
baserom.br.z64
```

Se a minha ROM for americana, posso usar `baserom.us.z64`. Para outra ROM, posso passar o caminho explicitamente com `--rom` e indicar a região com `--region` quando necessário.

Antes de trabalhar, faço uma cópia de segurança fora da pasta de edição. Nunca uso a única cópia da ROM como destino de uma reconstrução.

## 4. Preparar os assets da ROM

Ainda em `mk64-master`, execute:

```powershell
py .\PUBLIC_PREPARE_MK64_ASSETS.py
```

O script prepara e verifica os arquivos derivados da ROM que o projeto precisa. Essa preparação deve ser feita com a ROM escolhida já no lugar.

Se a preparação falhar, paro aqui e corrijo a causa. Não tento extrair ou reconstruir usando arquivos incompletos ou gerados para outra ROM.

## 5. Extrair as texturas

Execute a extração padrão:

```powershell
py .\EXTRACT_MK64_TEXTURES.py
```

Se eu quiser indicar a ROM e a região explicitamente:

```powershell
py .\EXTRACT_MK64_TEXTURES.py --rom .\baserom.br.z64 --region br
```

Exemplo para uma ROM americana:

```powershell
py .\EXTRACT_MK64_TEXTURES.py --rom .\baserom.us.z64 --region us
```

O extrator usa os metadados disponíveis nos YAMLs, JSONs e fontes do projeto para reconhecer símbolos, dimensões, formatos e outros dados. A região pode determinar quais metadados são encontrados. Se não existir uma pasta de metadados para a região indicada, o programa pode recorrer aos YAMLs dos EUA; isso é um *fallback* de metadados, não uma conversão da ROM para outra região.

### Opções úteis do extrator

- `--force`: sobrescreve PNGs que já existem. Uso isso apenas quando realmente quero refazer a extração, pois posso substituir edições manuais.
- `--no-karts`: não exporta os sprites de pilotos/karts.
- `--no-lakitu`: não exporta os quadros do Lakitu.
- `--no-generated`: não exporta as texturas identificadas nos bancos gerados do port.
- `--keep-black`: mantém o preto opaco nos casos tratados pelo extrator, em vez da transparência automática.
- `--out`: escolhe outra pasta de saída.
- `--dims`: fornece um JSON de dimensões manuais quando necessário.

Para consultar todas as opções disponíveis na versão que estou usando:

```powershell
py .\EXTRACT_MK64_TEXTURES.py --help
```

Depois da extração, guardo os manifests e os PNGs originais. Os manifests são parte do processo: eles relacionam imagens aos símbolos, formatos e dados da ROM. Não os apago nem os substituo por arquivos de outra extração.

## 6. Onde editar e como proteger cada categoria

Os nomes e as pastas exatos dependem dos assets encontrados na extração. Uso os nomes e caminhos que o extrator realmente criou; não renomeio arquivos ou diretórios por conveniência, porque o packer/repacker pode depender dos manifests e dos símbolos originais.

### Regra geral antes de editar

1. Faço uma cópia da pasta extraída antes de qualquer edição em lote.
2. Abro a imagem e confiro suas dimensões originais, transparência e aparência.
3. Identifico o tipo de recurso pelo caminho, pelo manifest e pelo formato; não deduzo a regra apenas pelo nome do PNG.
4. Se a imagem for indexada (CI4/CI8), verifico a paleta associada.
5. Preservo dimensões, enquadramento, transparência e conteúdo útil, a menos que o fluxo da categoria aceite comprovadamente uma mudança.
6. Salvo em PNG e mantenho o nome/caminho que o projeto espera.
7. Reconstruo primeiro uma pequena seleção de teste e valido no jogo antes de aplicar mudanças em massa.

### 6.1 Texturas comuns, objetos e itens

Incluem texturas compartilhadas pelo jogo, itens, efeitos, elementos de HUD e recursos usados em mais de um lugar.

- **Dimensões:** por padrão, mantenho as dimensões originais. Não aumento a resolução esperando que a ROM nativa passe a aceitar automaticamente a imagem maior.
- **Margem/padding:** não adiciono padding por padrão. Só mantenho ou acrescento margem se a imagem original já tiver uma área vazia que faça parte do mapeamento, ou se a definição daquele recurso indicar isso.
- **Paleta:** se o manifest indicar CI4/CI8 ou uma textura indexada, a paleta é parte do recurso. Não trato os índices como cores RGB comuns; é necessário preservar a correspondência de cores e índices esperada pela ROM.
- **Transparência:** mantenho o canal alfa e os pixels transparentes. Não preencho áreas transparentes com preto ou outra cor sem confirmar que esse recurso usa preto opaco.
- **Teste:** verifico o recurso em todos os lugares em que ele aparece, pois uma textura compartilhada pode afetar mais de uma tela ou pista.

### 6.2 Karts e sprites dos personagens durante a corrida

Os sprites de piloto/kart são organizados em quadros e podem usar paletas e sequências próprias.

- Não mudo a ordem, o nome nem a quantidade de quadros.
- Mantenho o tamanho de cada quadro e o enquadramento do kart/personagem.
- Não corto rodas, sombras, mãos, cabeça ou partes que pareçam encostar na borda.
- **Padding:** não acrescento margem arbitrariamente. Um deslocamento de poucos pixels pode fazer a animação tremer ou alterar a posição do sprite.
- **Paleta:** quando o manifest indica CI8/CI4, respeito a paleta correspondente ao personagem/quadro. Não substituo a paleta por uma paleta genérica nem reorganizo os índices.
- Se a arte for redesenhada em HD, mantenho o conteúdo dentro da área útil original e evito halos nas bordas transparentes.
- Testo vários personagens, curvas, colisões, animações e rodas; não considero suficiente testar apenas um piloto parado.

### 6.3 Lakitu e seus quadros animados

Os recursos de Lakitu podem conter vários quadros usados em momentos diferentes.

- Preservo a sequência e os nomes dos quadros.
- Não junto todos os quadros numa única imagem, a menos que o manifest e o carregador esperem explicitamente um atlas.
- **Padding:** mantenho a área e o alinhamento do quadro. Margens adicionais podem alterar o posicionamento ou o centro visual da animação.
- **Paleta:** respeito a paleta original quando o formato for indexado.
- Mantenho transparência limpa, sem bordas coloridas visíveis sobre o cenário.
- Testo os diferentes estados/gestos e não apenas um quadro.

### 6.4 Seleção de personagens, retratos e menus

Inclui retratos, imagens de seleção, fundos e elementos gráficos de menus.

- Mantenho cada arquivo no caminho e com o nome reconhecidos pelo JSON/manifest.
- **Padding:** em retratos e elementos posicionados por coordenadas, não adiciono margem externa sem verificar como o jogo calcula a posição. A área vazia que já existe na imagem pode fazer parte do alinhamento e deve ser preservada.
- Não estico o personagem para preencher a imagem: preservo proporção, centro visual e espaço em torno da cabeça/ombros.
- **Paleta:** imagens CI4/CI8 exigem a paleta original ou uma paleta comprovadamente compatível. Para imagens RGBA16/RGBA32, não há uma paleta indexada a preservar, mas ainda é necessário respeitar o formato esperado.
- Verifico a tela ao entrar e sair da seleção e ao voltar de outras telas. Se a imagem se deformar depois de navegar para trás, isso pode indicar problema de carregamento/estado ou associação, e não apenas um PNG mal editado.

### 6.5 Fundos e telas de menu

Fundos podem ter dimensões, recortes, mosaicos ou tratamentos de transparência específicos.

- Não suponho que um fundo possa ser substituído por uma imagem única maior.
- Preservo a composição e as proporções. Se o recurso for formado por faixas, tiles ou segmentos, edito cada parte que o manifest descreve.
- **Padding:** não crio bordas adicionais automaticamente. O espaço vazio pode ser parte do layout ou do recorte.
- **Paleta:** verifico o formato e o manifest; fundos indexados precisam respeitar a paleta. Não converto uma imagem indexada para RGB esperando que o jogo interprete o resultado da mesma forma.
- Testo todas as resoluções/modos de tela suportados pelo port e verifico se há repetição, costura ou recorte indevido.

### 6.6 TKMK00 — imagens comprimidas de menus

TKMK00 é um formato comprimido especial usado por algumas imagens do Mario Kart 64. Não deve ser tratado como um PNG comum nem como MIO0.

- Mantenho dimensões, proporção e conteúdo no retângulo esperado.
- **Padding:** não adiciono padding sem evidência no manifest. Tiles de TMEM, recortes e margens internas podem ser significativos.
- **Paleta:** não aplico a regra de CI4/CI8 automaticamente. TKMK00 tem seu próprio processo de decodificação e precisa ser reconstruído pelo fluxo específico.
- Não renomeio arquivos, não altero manifests manualmente e não copio metadados de uma ROM diferente.
- A reconstrução só deve ser considerada válida quando a ferramenta confirmar a codificação/validação e o recurso for testado no jogo. Se o codificador não conseguir manter o bloco dentro da alocação original, não forço a gravação.

### 6.7 Pistas, texturas de cenário e Rainbow Road

Texturas de pista podem ser compartilhadas, segmentadas em tiles ou carregadas por bancos diferentes. Rainbow Road também pode ter quadros/efeitos que não seguem o mesmo caminho das texturas comuns.

- Mantenho cada textura no diretório que o manifest e o packer reconhecem. Não crio níveis extras de pastas nem movo quadros para um diretório parecido sem atualizar e validar o fluxo.
- Preservo o mapeamento UV, a orientação, a repetição e as bordas que se encontram com tiles vizinhos.
- **Padding:** não acrescento margem por padrão. Em texturas repetidas/tiles, margens podem produzir linhas ou emendas; em texturas com coordenadas fixas, podem deslocar o conteúdo.
- **Paleta:** depende do formato indicado pelo manifest. Para CI4/CI8, respeitar a paleta é obrigatório; para RGBA16/RGBA32, a cor está codificada diretamente, mas o formato de entrada e o tamanho continuam importantes.
- Em Rainbow Road, verifico separadamente as texturas da pista e os quadros de efeitos/neon. A existência dos PNGs não prova que o carregador os está usando.
- Testo a pista em movimento, com diferentes ângulos e distâncias, procurando emendas, cintilação, textura original aparecendo por cima ou recursos que não mudaram.

### 6.8 Fontes, letras e números

Caracteres individuais podem ser pequenas texturas em I4, IA4, CI ou outro formato nativo. A dimensão e a posição dos pixels fazem parte do desenho da letra.

- Não redesenho a letra fora do retângulo original.
- **Padding:** não acrescento margem externa por conta própria. Em fontes pequenas, poucos pixels mudam o espaçamento e podem fazer a letra desaparecer ou encostar na seguinte.
- **Paleta/transparência:** respeito o formato e a interpretação de intensidade/alfa. Em formatos de intensidade, o valor do pixel pode representar brilho e não uma cor RGB.
- Não corrijo somente uma letra se houver indícios de que o problema afeta um conjunto de caracteres. Verifico as letras, números e símbolos relacionados e testo palavras completas em todas as telas relevantes.
- Se a reconstrução da ROM não conseguir reescrever com segurança o bloco comprimido ou alterar bytes vizinhos, mantenho o original e registro a limitação.

### 6.9 Frames, animações e sequências

- Preservo nomes, índices e ordem dos frames.
- Não troco uma sequência de imagens por uma folha de sprites sem suporte explícito no manifest.
- Mantenho as mesmas dimensões e o alinhamento de um frame para o seguinte.
- Verifico se os quadros estão realmente ligados ao carregador usado no jogo; um JSON correto, sozinho, não garante que o port os carregue.

## 7. Como saber se preciso respeitar paleta e padding

Uso esta regra rápida, confirmando sempre no manifest:

| Tipo/indicação do recurso | Paleta | Padding/margem | Regra segura |
|---|---|---|---|
| `CI4` / `CI8` (indexada) | **Sim, obrigatória** | Não adicionar sem comprovação | Preservar os índices e a TLUT associada |
| `I4` / `I8` (intensidade) | Não é paleta RGB; a intensidade importa | Não adicionar sem comprovação | Preservar intensidade, máscara e transparência |
| `IA4` / `IA8` (intensidade + alfa) | Não é paleta RGB; intensidade e alfa importam | Não adicionar sem comprovação | Preservar transparência e contraste |
| `RGBA16` / `RGBA32` | Não há paleta indexada | Não adicionar sem comprovação | Preservar formato, alfa e dimensões esperadas |
| `MIO0` | Depende do formato dos dados decodificados | Depende da textura e do mapeamento | Reconstruir somente se couber na alocação original |
| `TKMK00` | Fluxo especial; não presumir CI4/CI8 | Não adicionar sem comprovação | Usar ferramenta TKMK00 e validar o bloco |
| Sprites/frames/tiles | Conforme o formato real | Normalmente preservar exatamente o alinhamento | Não mudar ordem, posição ou dimensões sem validação |

**Importante:** o formato deve vir do manifest/metadados ou da fonte do projeto. Não o deduzo apenas pela aparência da imagem.

## 8. Reconstruir a ROM editada

O projeto inclui `ROM_REPACK_TEXTURES.py`, que trabalha com manifests e PNGs extraídos. Por segurança, ele primeiro permite validar as alterações sem gravar a ROM. O modo de gravação cria um arquivo de saída separado.

### 8.1 Primeiro, validar sem gravar

```powershell
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all
```

Esse comando é uma validação/prévia; não é para substituir a ROM original. Se estiver usando outra ROM, ajuste o argumento `--rom`.

Também posso limitar o teste a um tipo:

```powershell
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type mio0
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type tkmk00
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type lakitu
```

### 8.2 Depois, gravar uma ROM nova

Só depois de revisar a validação, executo:

```powershell
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all --patch --output .\baserom_MOD.z64
```

O resultado esperado é `baserom_MOD.z64`, mantendo `baserom.br.z64` intacta. Troque o nome do argumento `--rom` se a ROM-base tiver outro nome.

O repacker foi projetado para não realocar dados nem alterar ponteiros. Ele só deve gravar as substituições selecionadas quando elas couberem nas alocações comprimidas originais e passarem pela validação de ida e volta. Se uma textura não couber, der conflito ou falhar na validação, não tento contornar isso aumentando o bloco ou escrevendo por cima de dados vizinhos.

Para consultar as opções da versão atual:

```powershell
py .\ROM_REPACK_TEXTURES.py --help
```

## 9. Como testar sem perder o que já funciona

1. Guardo a ROM original e a extração limpa.
2. Faço uma alteração pequena em uma categoria.
3. Executo primeiro a validação sem `--patch`.
4. Se não houver erro, gero uma ROM de saída com outro nome.
5. Testo no emulador ou no ambiente de destino.
6. Confiro o recurso no contexto real, não apenas no visualizador de PNG.
7. Se algo parar de funcionar, volto à última ROM e aos últimos PNGs conhecidos como bons; não misturo mudanças experimentais com a base funcional.
8. Só depois de aprovar a mudança passo para a próxima categoria.

Uma imagem que não muda no jogo pode estar associada a outro símbolo, ser carregada por outro banco, usar outra paleta, fazer parte de um atlas ou não estar ligada ao caminho que está sendo editado. Antes de alterar o código de carregamento, confirmo o símbolo, o manifest, o hash e o recurso realmente carregado.

## 10. Limitações da ROM original

A principal limitação não é apenas do projeto de edição: é a estrutura da ROM e do código original. Muitas texturas estão em blocos comprimidos com espaço reservado fixo. Uma imagem editada pode ficar maior depois da compressão e não caber no espaço original, mesmo que visualmente esteja correta.

Também existem recursos com paletas compartilhadas, texturas divididas em tiles, quadros de animação, formatos de intensidade, dados comprimidos especiais e metadados que variam entre revisões da ROM. Por isso:

- Não existe uma regra universal de padding para todas as imagens.
- Nem toda textura pode aumentar de resolução dentro da ROM nativa.
- Nem toda imagem PNG exportada é necessariamente regravável pelo repacker.
- Não devo reutilizar offsets/manifests de uma ROM diferente sem verificar compatibilidade.
- Não considero uma textura corrigida até confirmar a alteração dentro do jogo.
- Se uma alteração exceder a alocação original, a solução pode exigir uma mudança técnica maior no carregador/alocação; não é seguro forçar a gravação.

É por isso que, das **4.061 texturas** contabilizadas no acompanhamento atual, **3.983 já funcionam** e **78 continuam sem solução confirmada**. O restante exige investigação individual, respeitando o que a ROM permite sem danificar as texturas que já estão corretas.

## 11. Compilar o port para Xbox 360

A reconstrução da ROM e a compilação do port são tarefas diferentes. Se eu também precisar compilar o projeto para Xbox 360, instalo o Visual Studio 2010 e a versão compatível das ferramentas Xbox 360 XDK/SDK, conforme a configuração do projeto.

Depois, abro a solução `MK64.sln` no ambiente compatível e compilo a configuração/plataforma de Xbox 360 definida pelo projeto. Os caminhos e versões do XDK podem variar; se a compilação falhar, uso o erro real do compilador para corrigir a dependência ou a configuração, sem substituir arquivos de código de forma indiscriminada.

A ROM reconstruída não substitui automaticamente o `MK64.xex`. Para testar no port, sigo o layout de arquivos esperado pela build usada e não suponho que um pacote de texturas externo seja necessário se o fluxo escolhido gravou as alterações diretamente na ROM.

## 12. Resumo do fluxo

Na pasta `mk64-master`, a sequência principal é:

```powershell
py -m pip install pillow
powershell -ExecutionPolicy Bypass -File ".\xbox360\setup_windows_asset_tools.ps1" -SkipTorch
py .\PUBLIC_PREPARE_MK64_ASSETS.py
py .\EXTRACT_MK64_TEXTURES.py --rom .\baserom.br.z64 --region br
# Editar os PNGs respeitando a categoria, o manifest, o formato e a paleta.
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all --patch --output .\baserom_MOD.z64
```

Se eu estiver usando outra ROM, altero `--rom` e `--region` conforme necessário. O nome do arquivo é uma convenção; a compatibilidade dos metadados e a estrutura real da ROM é que importam.

---

Este projeto é um trabalho de edição e reconstrução feito com cuidado: prefiro preservar as categorias que já funcionam e investigar as exceções individualmente, em vez de aplicar uma regra geral que possa quebrar o restante.

---

## Créditos

- **[n64decomp/mk64](https://github.com/n64decomp/mk64)** — descompilação original de Mario Kart 64, que serve de base para este projeto.
- **[sirdankz/MKart360](https://github.com/sirdankz/MKart360)** — port para Xbox 360, incluindo a compilação nativa, multiplayer entre consoles e correções de renderização. Este repositório é um fork desse projeto.
- **[ffeliph999](https://github.com/ffeliph999)** — solução dos problemas do telão e das arvores, além de melhorias gráficas e otimizações de desempenho.
