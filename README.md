# AutoWarp

**Warp e quantização automáticos para samples, loops e músicas.**
Criado por **DogSide Productions** e **Bluethedoggo**.

Você importa uma música ou outro áudio e mantém o arquivo original intacto. O AutoWarp só inicia a análise e o processamento quando você clica em **AUTO-WARP**. O motor de warping continua disponível para quantizar eventos de áudio na grade.

## O que ele faz

- **Importação não destrutiva**: LOAD e arrastar/soltar carregam e exibem o áudio original sem executar Auto-Warp.
- **Auto-Warp**: ao clicar no botão, detecta transientes, define o tamanho musical e prepara o áudio processado.
- **Quantize** com força (0–100 %), grade (1/4, 1/8, 1/16, 1/32, tercinas 1/8T e 1/16T) e **swing**.
- **Cortes editáveis**: adicione, mova e apague marcadores; arraste o marcador de baixo para posicionar um hit manualmente (sem snap com Shift).
- **START / END** arrastáveis para escolher a região do loop.
- **Warp modes**: *Beats* (mantém o pitch, preserva o ataque dos hits) e *Re-Pitch* (varispeed).
- **Segue o tempo do host**: o loop é re-renderizado se você mudar o BPM do projeto.
- **Playback**: *Host Sync* (toca travado na linha do tempo do host) ou *MIDI Trigger* (cada nota do piano roll dispara o loop do início).
- **Preview** para ouvir sem dar play no DAW, **Export WAV** do loop já warpado.
- O áudio, os cortes e as configurações são salvos **dentro do projeto** do DAW.
- Carrega WAV, AIFF, FLAC, MP3 e OGG (arrastar e soltar funciona). Nesta primeira etapa, o limite de duração continua em 10 minutos por causa do carregamento integral em memória.

## Como compilar o `.dll` (VST3) no Windows

O plugin é um **VST3**. No Windows, o `.dll` fica dentro da pasta `AutoWarp.vst3`
(`AutoWarp.vst3\Contents\x86_64-win\AutoWarp.vst3`). É isso que o FL Studio carrega.
Compile em Windows de um destes jeitos:

### Opção A: GitHub Actions (sem instalar nada)
1. Crie um repositório no GitHub e envie esta pasta.
2. Aba **Actions** → *Build AutoWarp (Windows VST3 / .dll)* → aguarde.
3. Baixe o artefato **AutoWarp-Windows-VST3** (contém `AutoWarp.vst3` e `AutoWarp-Standalone.exe`).
4. Antes de enviar a pasta ao GitHub, **apague a pasta `build`**, se existir.

### Opção B: no seu PC
1. Instale (grátis): Visual Studio 2022 Community com *Desktop development with C++*, [CMake](https://cmake.org/download/) e Git.
2. Dê dois cliques em `build_windows.bat` (baixa o JUCE sozinho).
3. O plugin fica em `build\AutoWarp_artefacts\Release\VST3\AutoWarp.vst3`.

## Instalar no FL Studio

> **IMPORTANTE:** a pasta **`AutoWarp.vst3`** precisa ficar exatamente em
> **`C:\Program Files\Common Files\VST3\`**. Fora dessa pasta o FL Studio pode não achar o plugin.

1. Descompacte o zip baixado.
2. Copie a pasta **`AutoWarp.vst3`** **inteira** (não só o arquivo de dentro, e sem pastas por fora) para:
   `C:\Program Files\Common Files\VST3\`
   O resultado final deve ser:
   `C:\Program Files\Common Files\VST3\AutoWarp.vst3\Contents\x86_64-win\AutoWarp.vst3`
3. Clique com o botão direito em `AutoWarp.vst3\Contents\x86_64-win\AutoWarp.vst3` → **Propriedades**. Se aparecer **Desbloquear**, marque e clique em OK (o Windows bloqueia arquivos baixados da internet).
4. FL Studio → **Options → Manage plugins**, marque **Verify plugins** e clique em **Find installed plugins**.
5. Procure **AutoWarp** em **Plugin database → Installed → Generators** e adicione ao Channel Rack.

### Problemas comuns
- **Não aparece no FL:** confira o caminho do passo 2, faça o passo 3 (desbloquear) e escaneie de novo (passo 4).
- **O Windows Defender acusou o `AutoWarp-Standalone.exe`:** é um falso positivo comum em programas novos sem assinatura digital. Você **não precisa** do `.exe` para usar o plugin no FL; pode apagá-lo. Se quiser conferir, envie o arquivo em virustotal.com.
- **"Não é um .dll":** no Windows, o VST3 é uma DLL com a extensão `.vst3`. O FL Studio só reconhece com essa extensão. O formato VST2 (`.dll` solto) não é suportado.

## Como usar
1. **LOAD** (ou arraste um áudio para a janela). O áudio original é carregado sem processamento.
2. Clique em **AUTO-WARP** somente quando quiser executar a análise e gerar o áudio processado.
3. Em cima fica o áudio original. Embaixo, o preview processado aparece depois do Auto-Warp.
4. Ajuste **QUANTIZE** (força), **Quantize grid** e **SWING**.
5. Se o tamanho do loop saiu errado, use **/2**, **x2** ou digite os beats em *Loop length (beats)*.
6. Refine os cortes:

| Ação | Como |
|---|---|
| Adicionar corte | duplo clique na faixa de cima |
| Mover corte | arrastar o marcador de cima |
| Apagar corte | clique direito (ou Alt+clique) no marcador |
| Colocar um hit em outro lugar | arrastar o marcador de **baixo** (Shift = sem snap na grade) |
| Voltar o marcador ao automático | clique direito no marcador de baixo |
| Mudar início / fim do loop | arrastar as linhas verdes START / END |
| Zoom / rolagem | roda do mouse / Shift + roda; **FIT** reseta |

7. **PREVIEW** toca o loop sem o DAW. **EXPORT WAV** salva o resultado.

> Para usar como o Quantize do piano roll: deixe *Playback = Host Sync*, **QUANTIZE = 100 %**, grade em 1/16 e dê play.
> Para tocar com notas: *Playback = MIDI Trigger* e escreva as notas no piano roll.

## Próxima arquitetura: AutoTimeStretch

O código DSP está sendo separado em `DogSideAudioCore`, uma biblioteca C++ compartilhada. O novo plugin **AutoTimeStretch** será um produto separado e reutilizará essa biblioteca, especialmente leitura/análise de áudio e o motor de renderização. O motor de mapa de andamento (`TempoEngine`) já possui uma primeira implementação e exportação MIDI; ainda falta conectar um beat tracker musical confiável à geração automática do mapa.

## Estrutura

```
Source/WarpEngine.*      motor de warping (detecção, quantização e render), sem dependência do JUCE
Source/TempoEngine.*     motor compartilhado de mapa de andamento + exportação MIDI, sem dependência do JUCE
DogSideAudioCore         biblioteca DSP compartilhada planejada para AutoWarp e AutoTimeStretch
Source/PluginProcessor.* parâmetros, thread de render, reprodução, estado do projeto
Source/PluginEditor.*    janela principal
Source/WarpView.*        forma de onda + cortes editáveis
tests/engine_test.cpp    teste automático do motor (g++ -O2 -std=c++17 -I../Source engine_test.cpp ../Source/WarpEngine.cpp)
```

## Créditos e licença
Criado por **DogSide Productions** e **Bluethedoggo**. Os créditos aparecem no rodapé da janela, no botão
**ABOUT** e como fabricante nos metadados do plugin. Veja `LICENSE.txt`: o plugin usa o framework JUCE
(AGPLv3 ou licença comercial); se for distribuir o binário, respeite a licença do JUCE.

## Tempo Quantize (new)

The **TEMPO MAP** action analyzes the loaded full-length recording and exports a Standard MIDI File containing tempo meta-events. The source audio is not stretched, cut, or rewritten. The existing **QUANTIZE** control sets how strongly detected beat positions are pulled toward the selected musical grid:

- **0%**: retain the detected timing deviations relative to the estimated average tempo (no beat snapping).
- **100%**: pull detected quarter-note beats fully onto the selected grid; the exported tempo map changes tempo so the source recording's detected beats land on those grid positions.
- Intermediate values blend between the two.

The first beat tracker uses an onset-energy envelope and autocorrelation. It is an initial automatic tracker, not a source-separation or instrument-aware transcription system. Dense arrangements, rubato, half/double-time ambiguity, long breakdowns, and weak transients can require manual verification. The MIDI contains tempo events only, not audio or notes. Import the MIDI tempo map into a DAW that supports tempo-map import, then place the untouched source audio at the same timeline origin.

## AutoTimeStretch (new plugin target)

`AutoTimeStretch` is a separate VST3 / Standalone product built from the same `DogSideAudioCore` library. It loads complete audio files, supports drag-and-drop, can estimate source BPM, stretches to a target BPM, offers pitch-preserving WSOLA or varispeed re-pitching, previews the render, and exports WAV. It does not replace AutoWarp; both products share the existing `WarpEngine` and tempo-analysis code.

The current AutoTimeStretch workflow is offline: load audio, set/detect source BPM, set target BPM, click **STRETCH**, then preview/export. Its pitch-preserving mode inherits the current WSOLA engine's quality and transient behavior; it is not yet a phase-vocoder or multi-resolution commercial mastering-grade algorithm. For best results, verify the source BPM and whether the track is in half-time or double-time before rendering.

Build both products with `build_windows.bat`. Outputs:

- `build\\AutoWarp_artefacts\\Release\\VST3\\AutoWarp.vst3`
- `build\\AutoTimeStretch_artefacts\\Release\\VST3\\AutoTimeStretch.vst3`

## DSP tests

- `tests/engine_test.cpp`: original warp engine regression tests.
- `tests/tempo_engine_test.cpp`: tempo-map generation, MIDI serialization, synthetic full-song beat tracking, and BPM bounds.

Both test executables can be built without JUCE:

```bash
g++ -O2 -std=c++17 -I Source tests/engine_test.cpp Source/WarpEngine.cpp -o engine_test
g++ -O2 -std=c++17 -I Source tests/tempo_engine_test.cpp Source/TempoEngine.cpp Source/WarpEngine.cpp -o tempo_engine_test
```
