paper-slider
============

`paper-slider` allows user to select a value from a range of values by
moving the slider thumb.  The interactive nature of the slider makes it a
great choice for settings that reflect intensity levels, such as volume,
brightness, or color saturation.

Example:

```html
<paper-slider></paper-slider>
```

Use `min` and `max` to specify the slider range. Default is `0` to `100`. For example:
```html
<paper-slider min="10" max="200" value="110"></paper-slider>
```

### Styling slider

To change the slider progress bar color:
```css
paper-slider {
  --paper-slider-active-color: #0f9d58;
}
```

To change the slider knob color:
```css
paper-slider {
  --paper-slider-knob-color: #0f9d58;
}
```

To change the slider pin color:
```css
paper-slider {
  --paper-slider-pin-color: #0f9d58;
}
```

To change the slider pin's font color:
```css
paper-slider {
  --paper-slider-pin-font-color: #0f9d58;
}
```

To change the slider secondary progress bar color:
```css
paper-slider {
  --paper-slider-secondary-color: #0f9d58;
}
```

To change the slider disabled active color:
```css
paper-slider {
  --paper-slider-disabled-active-color: #ccc;
}
```

To change the slider disabled secondary progress bar color:
```css
paper-slider {
  --paper-slider-disabled-secondary-color: #ccc;
}
```