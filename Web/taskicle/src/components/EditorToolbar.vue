<template>
  <NormalToolbar
    :title="'保存'"
    :disabled="disabled"
    @click="$emit('save')">
    <slot name="trigger">
      <Save :class="`${prefix}-icon`" />
    </slot>

    <div v-if="showToolbarName" :class="`${prefix}-toolbar-item-name`">
      保存
    </div>
  </NormalToolbar>
  <NormalToolbar
    :title="'保存草稿'"
    :disabled="disabled"
    @click="$emit('save-draft')">
    <slot name="trigger">
      <FilePen :class="`${prefix}-icon`" />
    </slot>

    <div v-if="showToolbarName" :class="`${prefix}-toolbar-item-name`">
      保存草稿
    </div>
  </NormalToolbar>
  <NormalToolbar
    :title="'查看页面版本'"
    :disabled="disabled"
    @click="$emit('open-history')">
    <slot name="trigger">
      <FileStack :class="`${prefix}-icon`" />
    </slot>

    <div v-if="showToolbarName" :class="`${prefix}-toolbar-item-name`">
      查看页面版本
    </div>
  </NormalToolbar>
  <NormalToolbar
    :title="'文本颜色'"
    :disabled="disabled">
    <slot name="trigger">
      <ElColorPicker
        v-model="color"
        show-alpha
        :predefine="presetColors"
        @change="onColorChange(true, $event)"
        size="small" />
    </slot>

    <div v-if="showToolbarName" :class="`${prefix}-toolbar-item-name`">
      文本颜色
    </div>
  </NormalToolbar>
  <NormalToolbar
    :title="'置入颜色值'"
    :disabled="disabled">
    <slot name="trigger">
      <ElColorPicker
        v-model="color"
        show-alpha
        :predefine="presetColors"
        @change="onColorChange(false, $event)"
        size="small" />
    </slot>

    <div v-if="showToolbarName" :class="`${prefix}-toolbar-item-name`">
      文本颜色
    </div>
  </NormalToolbar>
  <NormalToolbar
    :title="'在新窗口中打开预览'"
    :disabled="disabled"
    @click="onOpenNewPreview">
    <slot name="trigger">
      <SquareArrowOutUpRight :class="`${prefix}-icon`" />
    </slot>

    <div v-if="showToolbarName" :class="`${prefix}-toolbar-item-name`">
      在新窗口中打开
    </div>
  </NormalToolbar>
</template>

<script>
const commonProps = {
  insert: {
    type: Function,
    default: () => null
  },

  language: {
    type: String,
    default: ''
  },

  theme: {
    type: String,
    default: undefined
  },

  previewTheme: {
    type: String,
    default: undefined
  },

  codeTheme: {
    type: String,
    default: undefined
  },

  disabled: {
    type: Boolean,
    default: undefined
  },

  showToolbarName: {
    type: Boolean,
    default: undefined
  },

  title: {
    type: String,
    default: ''
  },

  trigger: {
    type: [String, Object],
    default: ''
  },

  editor: {
    default: undefined
  },

  color: {
    type: String,
  }
}
</script>
<script setup>
import { defineProps, defineOptions, defineEmits, ref } from 'vue'
import { SquareArrowOutUpRight, FilePen, FileStack, Save } from 'lucide-vue-next'
import { NormalToolbar } from 'md-editor-v3'

const prefix = 'md-editor'
const presetColors = [
  '#000000',
  '#262626',
  '#595959',
  '#8c8c8c',
  '#bfbfbf',

  '#ff4d4f', // red
  '#fa8c16', // orange
  '#fadb14', // yellow
  '#52c41a', // green
  '#13c2c2', // cyan
  '#1677ff', // blue
  '#722ed1'  // purple
]

const color = ref('')


const props = defineProps({
  ...commonProps
})

defineEmits(['save', 'save-draft', 'open-history'])

defineOptions({
  name: 'VavtMark'
})

const onColorChange = (isSpan, val) => {
  if (!val) return

  const generator = (selectedText) => {
    if (isSpan) {
      const prepend = `<span style="color: ${val}">`
      const append = `</span>`
  
      return {
        targetValue: prepend + `${selectedText}` + append,
        select: true,
        deviationStart: prepend.length,
        deviationEnd: -append.length
      }
    }
    else {
      return {
        targetValue: `${val}`
      }
    }
  }

  props.insert(generator)
}

const openPreviewInNewTab = (markdown) => {
  const win = window.open('', '_blank')
  if (!win) return

  win.document.write(`
    <!DOCTYPE html>
    <html>
      <head>
        <title>Markdown Preview</title>
        <meta charset="utf-8" />
      </head>
      <body>
        <div id="app"></div>
      </body>
    </html>
  `)
  win.document.close()

  win.__MD_CONTENT__ = markdown

  const script = win.document.createElement('script')
  script.type = 'module'
  script.innerHTML = `
    import { createApp, h } from 'vue'
    import { MdPreview } from 'md-editor-v3'

    createApp({
      render() {
        return h(MdPreview, {
          modelValue: window.__MD_CONTENT__
        })
      }
    }).mount('#app')
  `
  win.document.body.appendChild(script)
}

const onOpenNewPreview = () => {
  openPreviewInNewTab(props.editor?.value)
}
</script>