# Themes

The PCB canvas is fully themeable. W2R Open ships three built-in themes and lets you create,
edit and delete your own with a colour editor. User themes are stored in the **original app's
`pcb_themes.json` schema** and round-trip losslessly — including fields the renderer does not
model yet, so a file edited here stays compatible with the original.

## Built-in themes

- **Graphite**
- **Navy**
- **Plum**

Pick one from the toolbar **Theme:** dropdown or the **Theme** menu.

## Creating, editing, deleting

**Theme → Create Theme… / Edit Current Theme… / Delete Current Theme**.

The editor lets you set:

- the **name**, and
- a colour for every element — background, board fill, outline, part outline, component name
  text, pin / pin outline / pin text, ground pin, no-connect pin, net text, silkscreen, selected
  pin, same-net pin, ratsnest, part-highlight fill and border, and the **16 copper layers**.

Colours are chosen with a full colour dialog including an **alpha channel**, so translucent
silkscreens and highlights are possible.

## The `pcb_themes.json` schema

User themes are saved as a document of the form:

```json
{ "themes": [ { "name": "My Theme", "background": "#232323", "layer1": "#…", … } ] }
```

- Colours are `#rrggbb` (or `#aarrggbb` where alpha is used).
- The schema preserves **every** key found in the file, including ones the renderer does not use
  (e.g. `base`, `baseLayer`, `componentFill`, `refEdge`, `refPlate`, `overridePinColors`,
  `layer1…layer16`). This is verified by a round-trip test (`--theme-selftest`), which also checks
  that the original `ratsnet` key spelling survives.

## Where themes live

- Built-ins: compiled-in defaults (`src/resources/themes.json`).
- User themes: saved in application settings under the original schema. The original app keeps
  them next to the app as `config/pcb_themes.json`.

## Related

- [Views and Animations](Views-and-Animations.md) — canvas effects that layer on top of a theme.
