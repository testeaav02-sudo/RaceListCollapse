# Race List Collapse

Plugin para Kenshi + RE_Kenshi que recolhe listas longas de raças nas descrições de itens e permite expandi-las em páginas.

[English](README.md) · [Versões e downloads](https://github.com/testeaav02-sudo/RaceListCollapse/releases/latest) · [Validação, em inglês](docs/VALIDATION.md)

## Em quais itens aparecem os controles?

| Item | Lista compatível |
| --- | --- |
| Comidas restritas | A lista **Only for:** já gerada pelo jogo. Comidas comuns sem essa lista nativa não recebem uma lista nova. |
| Armas | Bônus e penalidades específicos de raças, definidos em **race damage**. Modificadores genéricos contra humanos, animais e robôs continuam visíveis separadamente. |
| Roupas e armaduras | Uma lista **Wearable by:** calculada a partir das restrições raciais e das regras de espaços de equipamento do jogo. |

**Os controles não aparecem em toda comida ou arma.** O plugin não cria uma lista universal de quem pode comer cada alimento nem uma lista de todas as raças para cada arma. A compatibilidade de armaduras descreve a raça; membros ausentes, espaços ocupados e outras condições do personagem ainda podem impedir o uso.

Os nomes vêm dos registros de raças e grupos carregados pelo jogo, incluindo mods. O plugin não traduz esses nomes e não depende de um mod de tradução. Cabeçalhos traduzidos pelo próprio jogo são reconhecidos quando disponíveis. A alteração é visual, sem mudar atributos de itens, regras das raças ou saves.

## Requisitos

Configuração testada:

- Kenshi **Steam 1.0.65**, Windows x64.
- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) **0.3.5** com **KenshiLib 0.5.0**.

Outras combinações, incluindo Kenshi 1.0.68, não foram verificadas. Kenshi, RE_Kenshi e suas bibliotecas não acompanham o pacote.

## Instalação

1. Feche o Kenshi e extraia um pacote da página de [versões](https://github.com/testeaav02-sudo/RaceListCollapse/releases/latest).
2. Copie a pasta `RaceListCollapse` do pacote para a pasta `mods` do Kenshi. Ela deve conter `RaceListCollapse.mod`, `RaceListCollapse.dll`, `RaceListCollapse.ini` e `RE_Kenshi.json`.
3. Habilite **RaceListCollapse** na aba Mods do launcher e inicie sua instalação que já utiliza RE_Kenshi.

Para atualizar, substitua os arquivos com o jogo fechado e confira o INI. O arquivo automático de código-fonte do GitHub serve para desenvolvimento, não para instalação.

## Uso

- Passe o mouse sobre um item compatível, mova até **Expand races** e clique.
- Use os botões **<** e **>** na tela para trocar de página; eles aparecem apenas quando há mais de uma página. **Collapse** recolhe a lista.
- **F8** alterna a expansão. **Page Up / Page Down** mudam a página.
- As quatro setas do teclado ficam reservadas ao jogo e não podem ser atribuídas ao plugin.
- Use os atalhos sem Ctrl, Shift ou Alt, com o jogo em foco e uma descrição compatível visível.

O padrão é de seis entradas por página, compartilhadas entre as seções do item. A preferência de expansão é compartilhada entre descrições durante a sessão. A contagem representa entradas exibidas, que podem incluir grupos de raças.

A descrição permanece acessível ao atravessar células vazias do inventário em direção aos controles. Parar sobre outro item permite trocar a descrição. Os botões mantêm a altura quando a última página tem menos entradas.

## Configuração e remoção

Edite `RaceListCollapse.ini` com o jogo fechado:

```ini
[General]
Enabled=1
PageSize=6
DebugLogging=0

[Keys]
Toggle=119
PreviousPage=33
NextPage=34
```

`PageSize` aceita 1–12. `Enabled=0` desativa o plugin. `DebugLogging=1` registra detalhes das descrições no log do RE_Kenshi. As teclas usam códigos virtuais do Windows em decimal. As setas direcionais (37–40) são rejeitadas; configurações inválidas restauram F8/Page Up/Page Down. Reinicie após alterar.

Para desinstalar, desabilite o mod no launcher e remova somente `mods/RaceListCollapse` com o jogo fechado. Não é necessário migrar saves.

## Validação e limites

Uma compilação local da versão 0.2.2 foi conferida no jogo com uma lista de armadura de **1.617 entradas em 270 páginas**, incluindo movimento lento até os controles, paginação, recolhimento, troca de item e reabertura do inventário. O binário público foi recompilado a partir do mesmo código-fonte, sem alterações, e **não foi testado novamente no jogo**. As listas de comida e arma foram observadas em um teste instrumentado de 0.2.0; **não foram testadas novamente no jogo em 0.2.2**.

Alguns caracteres asiáticos podem não aparecer na fonte do jogo. Listas de formato ambíguo continuam acessíveis por inteiro quando expandidas, mas podem não permitir paginação. A verificação mais recente não cobre teclado físico nem todas as combinações de mods externos. Consulte o [registro de validação](docs/VALIDATION.md).

## Compilação

Use **Visual C++ 2010 x64 (VC100)**, **Windows SDK 7.1** e as [dependências oficiais dos exemplos KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps), obtidas com Git LFS. Um compilador recente não substitui essa compatibilidade binária. Extraia o arquivo do Boost incluído nas dependências antes de compilar.

```powershell
.\build.ps1 -DepsRoot 'D:\SDK\KenshiLib_Examples_deps' -VcRoot 'D:\SDK\VC100\VC' -SdkRoot 'D:\SDK\Windows\v7.1'
```

O resultado é `build/RaceListCollapse.dll`. Acrescente `-Test` para compilar e executar os quatro programas de teste. Ferramentas portáteis com headers separados podem usar `-VcIncludeRoot`; `-OutputDirectory` muda o destino. O script habilita `/MD`, `/GL` e `/LTCG` e não instala arquivos no jogo.

## Licença

Código do plugin: [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html). Kenshi e dependências de terceiros mantêm suas próprias licenças e são obtidos separadamente. Desenvolvido com [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) e seus [exemplos de plugins](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples).
