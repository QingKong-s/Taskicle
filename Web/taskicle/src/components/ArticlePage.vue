<template>
  <div class="article-page">
    <div class="left" :style="{ width: leftWidth + 'px' }">
      <div class="left-header">
        {{ currentGroupName }}
        <el-button
          style="float: right;"
          type="text"
          size="small"
          @click="onAddPage">
          <el-icon><Plus /></el-icon>
          新建页面
        </el-button>
      </div>
      <ul class="list">
        <li v-for="item in pages" :key="item.page_id"
          :class="['click-item', { 'selected': selectedPage && (selectedPage.page_id === item.page_id) }]"
          @click="selectPage(item)">
          <div class="row">
            <span class="capsule id-capsule">#{{ item.page_id }}</span>
            <span class="task-name" :title="item.page_name">{{ item.page_name }}</span>
            <div class="actions">
              <el-button type="text" size="small" @click.stop="onEditPage(item)">
                <el-icon><i class="el-icon-edit"></i></el-icon>
              </el-button>
              <el-button type="text" size="small" @click.stop="onDeletePage(item)">
                <el-icon><i class="el-icon-delete"></i></el-icon>
              </el-button>
            </div>
          </div>
        </li>
      </ul>
    </div>

    <Splitter @ondrag="onDrag" />

    <div class="right">
      <div class="editor-topbar">
        <span v-if="loadedIsDraft" class="draft-badge">草稿</span>
        <span class="page-title">
          {{ selectedPage ? selectedPage.page_name : '未选择页面' }}
        </span>
        <span class="title-button">
        </span>
      </div>
      <MdEditor
        class="editor"
        ref="editorRef"
        v-model="editorContent"
        :noPrettier="true"
        :toolbars="toolbars">
        <template #defToolbars>
          <Mark title="高亮"></Mark>
          <EditorToolbar
            :editor="editorContent"
            @save="savePage(false)"
            @save-draft="savePage(true)"
            @open-history="openHistory">
          </EditorToolbar>
        </template>
      </MdEditor>
      <PageHistoryModal ref="historyModal" :pageId="selectedPage ? selectedPage.page_id : null" />
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted } from 'vue'
import Splitter from './SplitBar.vue'
import api from '@/utils/api'
import { useRoute, useRouter } from 'vue-router'
import { ElMessageBox, ElMessage } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'
import { showAxiosErrorMessage, showErrorMessage } from '@/utils/utils'
import { MdEditor, config } from 'md-editor-v3'
import 'md-editor-v3/lib/style.css'
import TableMergeCells from "markdown-it-table-merge-cells"
import pako from 'pako'
import PageHistoryModal from './PageHistoryModal.vue'
import { Mark } from '@vavt/v3-extension'
import '@vavt/v3-extension/lib/asset/Mark.css';
import MarkExtension from 'markdown-it-mark';
import EditorToolbar from './EditorToolbar.vue'

const leftWidth = ref(300)

const route = useRoute()
const router = useRouter()

const pages = ref([])
const currentGroup = ref(route.query.group || '')
const currentGroupName = ref('')
const selectedPage = ref(null)

const editorRef = ref();
const editorContent = ref('')
const loadedIsDraft = ref(false)
const toolbars = [
  'bold',
  'underline',
  'italic',
  0,
  '-',
  'title',
  'strikeThrough',
  'sub',
  'sup',
  'quote',
  'unorderedList',
  'orderedList',
  'task',
  '-',
  'codeRow',
  'code',
  'link',
  'image',
  'table',
  'mermaid',
  'katex',
  '-',
  'revoke',
  'next',
  '-',
  1,
  '=',
  'pageFullscreen',
  'fullscreen',
  'preview',
  'previewOnly',
  'htmlPreview',
];

config({
  markdownItPlugins(plugins) {
    return [
      ...plugins,
      {
        type: 'mergeCells',
        plugin: TableMergeCells,
        options: {},
      },
      {
        type: 'markExt',
        plugin: MarkExtension,
        options: {},
      },
    ];
  },
});

async function fetchGroupName(groupId) {
  if (!groupId) return
  try {
    const res = await api.get('/api/page_group_list', { params: { count: 100, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      const groups = j.data || []
      const found = groups.find(g => String(g.page_group_id) === String(groupId))
      if (found) currentGroupName.value = found.group_name || ''
    }
  } catch (e) {
    currentGroupName.value = ''
  }
}

async function fetchPages(groupId) {
  if (!groupId) {
    pages.value = []
    return
  }
  try {
    const res = await api.get('/api/page_list', { params: { group_id: groupId, count: 100, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      pages.value = j.data || []
      if (pages.value.length) {
        const pageId = route.query.page
        if (pageId) {
          const page = pages.value.find(item => String(item.page_id) === String(pageId))
          selectedPage.value = page || pages.value[0]
        } else {
          selectedPage.value = pages.value[0]
        }
      } else {
        selectedPage.value = null
      }
      // 自动加载首个页面内容（若存在草稿，后端会优先返回草稿）
      if (selectedPage.value) loadPage(selectedPage.value)
    } else {
      pages.value = []
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

onMounted(() => {
  fetchPages(currentGroup.value)
  fetchGroupName(currentGroup.value)
})

onUnmounted(() => {
  /* */
})

watch(() => route.query.group, (g) => {
  currentGroup.value = g || ''
  fetchPages(currentGroup.value)
  fetchGroupName(currentGroup.value)
})

watch(() => route.query.page, (p) => {
  if (p && pages.value.length) {
    const page = pages.value.find(item => String(item.page_id) === String(p))
    if (page) {
      selectPage(page)
    }
  }
})

function selectPage(item) {
  selectedPage.value = item
  loadPage(item)
  if (item) {
    router.push({ name: 'articles', query: { group: currentGroup.value, page: item.page_id } })
  }
}

async function loadPage(item) {
  if (!item) {
    editorContent.value = ''
    loadedIsDraft.value = false
    return
  }
  try {
    const res = await api.get('/api/page_load',
      { params: { page_id: item.page_id }, responseType: 'arraybuffer' })
    const arr = res.data
    const parsed = parsePageBuffer(arr)
    loadedIsDraft.value = !!parsed.bTemp
    editorContent.value = parsed.content || ''
    editorRef.value?.resetHistory()
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
    editorContent.value = ''
    loadedIsDraft.value = false
    editorRef.value?.resetHistory()
  }
}

function parsePageBuffer(buf) {
  if (!buf || buf.byteLength < 36) return {}
  const dv = new DataView(buf)
  let off = 0
  const Magic = dv.getUint32(off, true); off += 4
  const iPageId = dv.getInt32(off, true); off += 4
  const bTemp = dv.getInt32(off, true); off += 4
  const bCompressed = dv.getInt32(off, true); off += 4
  const eType = dv.getInt32(off, true); off += 4
  const crc32 = dv.getUint32(off, true); off += 4
  const cbContent = dv.getUint32(off, true); off += 4
  const r = dv.getInt32(off, true); off += 4
  const r2 = dv.getUint32(off, true); off += 4
  const contentBytes = new Uint8Array(buf, off, cbContent || 0)
  let content = ''
  try {
    if (bCompressed) {
      const decompressed = pako.ungzip(contentBytes)
      content = new TextDecoder().decode(decompressed)
    } else {
      content = new TextDecoder().decode(contentBytes)
    }
  } catch (e) {
    console.error('parsePageBuffer decode error', e)
    content = ''
  }
  return { Magic, iPageId, bTemp, bCompressed, eType, crc32, cbContent, r, r2, content }
}

function buildPageBuffer(pageId, contentStr, isDraft) {
  const contentBytes = new TextEncoder().encode(contentStr || '')
  const headerSize = 36
  const total = headerSize + contentBytes.length
  const ab = new ArrayBuffer(total)
  const dv = new DataView(ab)
  let off = 0
  dv.setUint32(off, 0xDEADBEEF, true); off += 4
  dv.setInt32(off, pageId || 0, true); off += 4
  dv.setInt32(off, isDraft ? 1 : 0, true); off += 4
  dv.setInt32(off, 0, true); off += 4 // bCompressed = 0
  dv.setInt32(off, 1, true); off += 4 // eType = 1
  dv.setUint32(off, crc32(contentBytes), true); off += 4
  dv.setUint32(off, contentBytes.length, true); off += 4
  dv.setInt32(off, 0, true); off += 4 // r
  dv.setUint32(off, 0, true); off += 4 // r2
  const payload = new Uint8Array(ab, off, contentBytes.length)
  payload.set(contentBytes)
  return ab
}

// CRC32 implementation (standard)
function crc32(buf) {
  const table = crc32.table || (crc32.table = makeCrcTable())
  let crc = 0 ^ (-1)
  for (let i = 0; i < buf.length; i++) {
    crc = (crc >>> 8) ^ table[(crc ^ buf[i]) & 0xFF]
  }
  return (crc ^ (-1)) >>> 0
}

function makeCrcTable() {
  let c
  const table = new Uint32Array(256)
  for (let n = 0; n < 256; n++) {
    c = n
    for (let k = 0; k < 8; k++) {
      c = ((c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1))
    }
    table[n] = c >>> 0
  }
  return table
}

async function savePage(asDraft) {
  if (!selectedPage.value) {
    ElMessage.warning('未选择页面')
    return
  }
  try {
    const ab = buildPageBuffer(selectedPage.value.page_id, editorContent.value || '', asDraft)
    const res = await api.post('/api/page_save', ab,
    { headers: { 'Content-Type': 'application/octet-stream' } })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success(asDraft ? '草稿保存成功' : '保存成功')
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

function openHistory() {
  if (!selectedPage.value) return
  const modal = historyModal.value
  if (modal && modal.open) modal.open()
}

const historyModal = ref(null)

async function onAddPage() {
  if (!currentGroup.value) {
    ElMessage.warning('请先选择页面组')
    return
  }
  try {
    const { value } = await ElMessageBox.prompt('输入页面名称', '新建页面', {
      confirmButtonText: '创建',
      cancelButtonText: '取消'
    })
    const name = (value || '').trim()
    const id = parseInt(currentGroup.value)
    const res = await api.post('/api/page_insert',
      { page_group_id: id, page_name: name })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('创建成功')
      await fetchPages(currentGroup.value)
      const newId = j.data.page_id
      selectPage(newId)
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onEditPage(p) {
  try {
    const { value } = await ElMessageBox.prompt('输入页面名称', '编辑页面', {
      confirmButtonText: '保存',
      cancelButtonText: '取消',
      inputValue: p.page_name || ''
    })
    const name = (value || '').trim()
    if (!name) {
      ElMessage.warning('名称不能为空')
      return
    }
    const res = await api.post('/api/page_update', { page_id: p.page_id, page_name: name })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('修改成功')
      await fetchPages(currentGroup.value)
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function onDeletePage(p) {
  try {
    await ElMessageBox.confirm(`确认删除页面：${p.page_name} (${p.page_id})？`, '删除页面', {
      confirmButtonText: '删除',
      cancelButtonText: '取消',
      type: 'warning'
    })
    const res = await api.post('/api/page_delete', { page_id: p.page_id })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('删除成功')
      await fetchPages(currentGroup.value)
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

function onDrag(movementX) {
  leftWidth.value = Math.max(300, leftWidth.value + movementX)
}
</script>

<style scoped>
.article-page {
  display: flex;
  height: 100%;
}

.left {
  width: 360px;
  border-right: 1px solid #ebeef5;
  background: #fff;
  display: flex;
  flex-direction: column;
}

.left-header {
  margin-top: 1px;
  padding: 12px;
  font-weight: 600;
  vertical-align: middle;
  border-bottom: 1px solid #f5f7fa;
}

.list {
  padding: 8px;
  overflow-y: auto;
}

.click-item {
  padding: 8px 4px;
  cursor: pointer;
  border-radius: 4px;
}

.click-item:hover {
  background: #f5f7fa;
}

.row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0px 6px;
  font-size: 14px;
}

.capsule {
  padding: 3px 10px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 500;
  white-space: nowrap;
}

.id-capsule {
  background: #e8f4ff;
  color: #1e80ff;
}

.task-name {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.actions {
  display: flex;
  gap: 6px;
}

.selected {
  background: #e6f4ff;
}

.right {
  flex: 1;
  padding: 12px;
  padding-left: 0;
  display: flex;
  flex-direction: column;
  min-height: 0;
  font-size: 20px;
}

.editor {
  height: 100%;
}

.editor-topbar {
  height: 40px;
}

.title-button {
  float: right;
}

:deep() .md-editor-preview code {
  font-family: source-code-pro, Menlo, Monaco, Consolas, Courier New, monospace !important;
}

:deep()
.cm-editor, 
.cm-scroller, 
.cm-content, 
.cm-content *, 
.cm-line 
{
  font-size: 20px !important;
  line-height: 24px !important;
}

:deep()
.md-editor .ͼ1 .cm-scroller,
.md-editor .cm-scroller .cm-line {
  font-size: 20px !important;
  line-height: 24px !important;
}

:deep()
.md-editor-custom-scrollbar__track {
  width: 12px !important;
}
:deep()
.md-editor-custom-scrollbar__thumb {
  width: 12px !important;
}
</style>