<template>
  <div class="article-page">
    <div class="left" :style="{ width: leftWidth + 'px' }">
      <div class="left-header">
        {{ currentGroupName }}
        <el-button style="float: right;" type="text" size="small" @click="onAddPage">
          <el-icon>
            <Plus />
          </el-icon>
          新建页面
        </el-button>
      </div>
      <ul class="list" @scroll="onListScroll">
        <li v-for="item in pages" :key="item.page_id"
          :class="['click-item', { 'selected': selectedPage && (selectedPage.page_id === item.page_id) }]"
          @click="selectPage(item)">
          <div class="row">
            <span class="capsule id-capsule">#{{ item.page_id }}</span>
            <span class="task-name" :title="item.page_name">{{ item.page_name }}</span>
            <div class="actions">
              <el-button class="edit-btn" type="text" size="small" @click.stop="onEditPage(item)">
                <el-icon>
                  <Edit />
                </el-icon>
              </el-button>
              <el-button class="edit-btn" type="text" size="small" @click.stop="onDeletePage(item)">
                <el-icon>
                  <Delete />
                </el-icon>
              </el-button>
            </div>
          </div>
        </li>
        <li v-if="listLoading" class="loading-text">加载中...</li>
        <li v-if="listFinished && pages.length > 0" class="loading-text" style="color: #ccc">没有更多了</li>
      </ul>
    </div>

    <Splitter @ondrag="onDrag" />

    <div class="right">
      <div class="editor-topbar">
        <el-tag v-if="loadedIsDraft" type="warning" size="small" effect="dark" class="draft-badge">草稿</el-tag>
        <span class="page-title">
          {{ selectedPage ? selectedPage.page_name : '未选择页面' }}
          <span v-if="isUnsaved" class="unsaved-mark" title="有未保存的更改">*</span>
        </span>
      </div>

      <MdEditor class="editor" ref="editorRef" v-model="editorContent" :noPrettier="true" :toolbars="toolbars"
        @onChange="handleInput" :mdHeadingId="mdHeadingId" @onRemount="handleRemount">
        <template #defToolbars>
          <Mark title="高亮"></Mark>
          <EditorToolbar :editor="editorContent" @save="savePage(false)" @save-draft="savePage(true)"
            @open-history="openHistory">
          </EditorToolbar>
        </template>
      </MdEditor>

      <PageHistoryModal ref="historyModal" :pageId="selectedPage ? selectedPage.page_id : null"
        @restore="handleRestoreContent" />
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted } from 'vue'
import Splitter from './SplitBar.vue'
import api from '@/utils/api'
import { useRoute, useRouter } from 'vue-router'
import { ElMessageBox, ElMessage } from 'element-plus'
import { Plus, Edit, Delete } from '@element-plus/icons-vue'
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

const mdHeadingId = ({ text }) => `${text}`;
const leftWidth = ref(300)
const route = useRoute()
const router = useRouter()
const pages = ref([])
const currentGroup = ref(route.query.group || '')
const currentGroupName = ref('')
const selectedPage = ref(null)
const editorRef = ref()
const editorContent = ref('')
const loadedIsDraft = ref(false)
const historyModal = ref(null)
const noAutoSave = ref(false)
const isUnsaved = ref(false)
const listPage = ref(0)
const listLoading = ref(false)
const listFinished = ref(false)
const PAGE_SIZE = 50
const scrollHash = ref(false)

let autoSaveTimer = null

onMounted(() => {
  fetchPages(currentGroup.value, true)
  fetchGroupName(currentGroup.value)
})

onUnmounted(() => {
  if (autoSaveTimer) clearTimeout(autoSaveTimer)
})

function scrollToHash() {
  const hash = window.location.hash;
  if (!hash) return;
  const targetId = decodeURIComponent(hash.substring(1));
  const previewContainer = document.querySelector('.md-editor-preview-wrapper');
  const targetEl = document.getElementById(targetId);
  if (targetEl && previewContainer) {
    targetEl.scrollIntoView({ behavior: 'smooth', block: 'start' });
  }
}
function handleRemount() {
  if (!scrollHash.value) return
  scrollToHash()
  scrollHash.value = false
}

function handleInput() {
  if (autoSaveTimer) clearTimeout(autoSaveTimer)
  if (noAutoSave.value) {
    noAutoSave.value = false
    scrollHash.value = true
  }
  else {
    isUnsaved.value = true
    if (!loadedIsDraft.value) {
      loadedIsDraft.value = true
    }
    editorRef.value?.resetHistory()
    autoSaveTimer = setTimeout(() => {
      autoSavePage()
    }, 3000)
  }
}

async function autoSavePage() {
  if (noAutoSave.value || !selectedPage.value || !editorContent.value) {
    return
  }
  try {
    const ab = buildPageBuffer(selectedPage.value.page_id, editorContent.value, true)
    await api.post('/api/page_save', ab, {
      headers: { 'Content-Type': 'application/octet-stream' }
    })
    ElMessage.info('自动保存草稿成功')
    isUnsaved.value = false
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

async function savePage(asDraft) {
  if (!selectedPage.value) {
    ElMessage.warning('未选择页面')
    return
  }
  if (autoSaveTimer) clearTimeout(autoSaveTimer)

  try {
    const ab = buildPageBuffer(selectedPage.value.page_id, editorContent.value || '', asDraft)
    const res = await api.post('/api/page_save', ab, {
      headers: { 'Content-Type': 'application/octet-stream' }
    })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success(asDraft ? '草稿保存成功' : '正式版本保存成功')
      if (!asDraft) {
        loadedIsDraft.value = false
      }
      isUnsaved.value = false
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
  }
}

function handleRestoreContent(content) {
  editorContent.value = content
  loadedIsDraft.value = true
  isUnsaved.value = true 
  ElMessage.info('已恢复历史版本内容，请记得保存正式版')
}

async function loadPage(item) {
  if (autoSaveTimer) clearTimeout(autoSaveTimer)
  isUnsaved.value = false
  
  if (!item) {
    noAutoSave.value = true
    editorContent.value = ''
    loadedIsDraft.value = false
    return
  }
  try {
    const res = await api.get('/api/page_load', {
      params: { page_id: item.page_id, is_draft: 1 },
      responseType: 'arraybuffer'
    })
    const parsed = parsePageBuffer(res.data)
    if (parsed.r) {
      showErrorMessage(ElMessage, { r: parsed.r, r2: parsed.r2, err_msg: '' })
    }
    else {
      loadedIsDraft.value = !!parsed.bTemp
      noAutoSave.value = true
      isUnsaved.value = false
      editorContent.value = parsed.content || ''
    }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
    noAutoSave.value = true
    isUnsaved.value = false
    editorContent.value = ''
    loadedIsDraft.value = false
  }
}

const toolbars = ['bold', 'underline', 'italic', 0, '-', 'title', 'strikeThrough', 'sub', 'sup', 'quote', 'unorderedList', 'orderedList', 'task', '-', 'codeRow', 'code', 'link', 'image', 'table', 'mermaid', 'katex', '-', 'revoke', 'next', '-', 1, '=', 'pageFullscreen', 'fullscreen', 'preview', 'previewOnly', 'htmlPreview', 'catalog'];

config({
  markdownItPlugins(plugins) {
    return [
      ...plugins,
      { type: 'mergeCells', plugin: TableMergeCells, options: {} },
      { type: 'markExt', plugin: MarkExtension, options: {} },
    ];
  },
});

function parsePageBuffer(buf) {
  if (!buf || buf.byteLength < 36) return {}
  const dv = new DataView(buf)
  let off = 0
  const Magic = dv.getUint32(off, true); off += 4
  const iPageId = dv.getInt32(off, true); off += 4
  const bTemp = dv.getInt32(off, true); off += 4
  const bCompressed = dv.getInt32(off, true); off += 4
  const eType = dv.getInt32(off, true); off += 4
  const crc32Val = dv.getUint32(off, true); off += 4
  const cbContent = dv.getUint32(off, true); off += 4
  const r = dv.getInt32(off, true); off += 4
  const r2 = dv.getUint32(off, true); off += 4
  const contentBytes = new Uint8Array(buf, off, cbContent || 0)

  eType
  let content = ''
  try {
    if (bCompressed) {
      content = new TextDecoder().decode(pako.ungzip(contentBytes))
    } else {
      content = new TextDecoder().decode(contentBytes)
    }
  } catch (e) { content = '' }
  if (content.length) {
    const crc32ValActual = crc32(contentBytes)
    if (crc32Val != crc32ValActual) {
      ElMessage.error('内容校验失败')
      content = ''
    }
  }
  return { Magic, iPageId, bTemp, content, r, r2 }
}

function buildPageBuffer(pageId, contentStr, isDraft) {
  const contentBytes = new TextEncoder().encode(contentStr || '')
  const ab = new ArrayBuffer(36 + contentBytes.length)
  const dv = new DataView(ab)
  let off = 0
  dv.setUint32(off, 0xDEADBEEF, true); off += 4
  dv.setInt32(off, pageId || 0, true); off += 4
  dv.setInt32(off, isDraft ? 1 : 0, true); off += 4
  dv.setInt32(off, 0, true); off += 4
  dv.setInt32(off, 1, true); off += 4
  dv.setUint32(off, crc32(contentBytes), true); off += 4
  dv.setUint32(off, contentBytes.length, true); off += 4
  dv.setInt32(off, 0, true); off += 4
  dv.setUint32(off, 0, true); off += 4
  new Uint8Array(ab, 36).set(contentBytes)
  return ab
}

function crc32(buf) {
  const table = crc32.table || (crc32.table = makeCrcTable())
  let crc = -1
  for (let i = 0; i < buf.length; i++) {
    crc = (crc >>> 8) ^ table[(crc ^ buf[i]) & 0xFF]
  }
  return (crc ^ -1) >>> 0
}
function makeCrcTable() {
  let c; const table = new Uint32Array(256)
  for (let n = 0; n < 256; n++) {
    c = n; for (let k = 0; k < 8; k++) {
      c = ((c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1))
    }
    table[n] = c >>> 0
  }
  return table
}

async function fetchGroupName(groupId) {
  if (!groupId) return
  try {
    const res = await api.get('/api/page_group_list', { params: { count: 100, page: 0 } })
    if (res.data?.r === 0) {
      const found = res.data.data.find(g => String(g.page_group_id) === String(groupId))
      if (found) currentGroupName.value = found.group_name || ''
    }
  } catch (e) { /* */ }
}

async function fetchPages(groupId, reset = false) {
  if (!groupId) { 
    pages.value = []
    return 
  }
  
  if (reset) {
    listPage.value = 0
    listFinished.value = false
    pages.value = []
  }

  if (listLoading.value || (listFinished.value && !reset)) return;

  listLoading.value = true

  try {
    const res = await api.get('/api/page_list', { 
      params: { 
        group_id: groupId, 
        count: PAGE_SIZE, 
        page: listPage.value 
      } 
    })
    
    if (res.data?.r === 0) {
      const newData = res.data.data || []
      
      if (reset) {
        pages.value = newData
      } else {
        pages.value.push(...newData)
      }

      if (newData.length < PAGE_SIZE) {
        listFinished.value = true
      } else {
        listPage.value++
      }

      if (reset) {
        const pageId = route.query.page
        if (pageId) {
          selectedPage.value = pages.value.find(item => String(item.page_id) === String(pageId)) || pages.value[0]
        } else {
          selectedPage.value = pages.value[0]
        }
        if (selectedPage.value) loadPage(selectedPage.value)
      }
    }
  } catch (e) { 
    showAxiosErrorMessage(ElMessage, e) 
  } finally {
    listLoading.value = false
  }
}

function onListScroll(e) {
  const { scrollTop, scrollHeight, clientHeight } = e.target;
  if (scrollTop + clientHeight >= scrollHeight - 20) {
    fetchPages(currentGroup.value, false)
  }
}

function selectPage(item) {
  selectedPage.value = item
  loadPage(item)
  if (item) router.push({ query: { ...route.query, page: item.page_id } })
}

watch(() => route.query.group, (g) => {
  currentGroup.value = g || ''
  fetchPages(currentGroup.value, true)
  fetchGroupName(currentGroup.value)
})

function openHistory() {
  if (!selectedPage.value) return
  historyModal.value?.open()
}

async function onAddPage() {
  if (!currentGroup.value) return ElMessage.warning('请选择页面组')
  try {
    const { value } = await ElMessageBox.prompt('输入页面名称', '新建页面')
    const res = await api.post('/api/page_insert', { page_group_id: parseInt(currentGroup.value), page_name: value })
    if (res.data?.r === 0) {
      ElMessage.success('创建成功')
      await fetchPages(currentGroup.value, true)
      const newPage = pages.value.find(p => p.page_id === res.data.data.page_id)
      if (newPage) selectPage(newPage)
    }
  } catch (e) { /* */ }
}

async function onEditPage(p) {
  try {
    const { value } = await ElMessageBox.prompt('输入页面名称', '编辑页面', { inputValue: p.page_name })
    const res = await api.post('/api/page_update', { page_id: p.page_id, page_name: value })
    if (res.data?.r === 0) {
      ElMessage.success('修改成功')
      const target = pages.value.find(item => item.page_id === p.page_id)
      if (target) target.page_name = value
      if (selectedPage.value && selectedPage.value.page_id === p.page_id) {
        selectedPage.value.page_name = value
      }
    }
  } catch (e) { /* */ }
}

async function onDeletePage(p) {
  try {
    await ElMessageBox.confirm(`确认删除页面：${p.page_name}？`, '提示', { type: 'warning' })
    const res = await api.post('/api/page_delete', { page_id: p.page_id })
    if (res.data?.r === 0) {
      ElMessage.success('删除成功')
      fetchPages(currentGroup.value, true)
    }
  } catch (e) { /* */ }
}

function onDrag(movementX) {
  leftWidth.value = Math.max(250, leftWidth.value + movementX)
}
</script>

<style scoped>
.article-page {
  display: flex;
  height: 100%;
  width: 100%;
  overflow: hidden;
}

.left {
  border-right: 1px solid #ebeef5;
  background: #fff;
  display: flex;
  flex-direction: column;
}

.left-header {
  padding: 12px;
  font-weight: 600;
  border-bottom: 1px solid #f5f7fa;
}

.list {
  padding: 8px;
  overflow-y: auto;
  list-style: none;
  margin: 0;
}

.click-item {
  padding: 8px 4px;
  cursor: pointer;
  border-radius: 4px;
}

.click-item:hover {
  background: #f5f7fa;
}

.selected {
  background: #e6f4ff !important;
}

.row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 6px;
  font-size: 14px;
}

.capsule {
  padding: 2px 8px;
  border-radius: 10px;
  font-size: 11px;
  background: #e8f4ff;
  color: #1e80ff;
}

.task-name {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.actions {
  display: none;
}

.click-item:hover .actions {
  display: flex;
}

.loading-text {
  text-align: center;
  font-size: 12px;
  color: #909399;
  padding: 10px 0;
}

.unsaved-mark {
  color: #f56c6c;
  margin-left: 4px;
  font-size: 20px;
  line-height: 1;
  vertical-align: middle;
}

.right {
  flex: 1;
  padding: 12px;
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.editor {
  flex: 1;
}

.editor-topbar {
  height: 40px;
  display: flex;
  align-items: center;
  gap: 10px;
}

.draft-badge {
  font-weight: bold;
}

.page-title {
  font-size: 18px;
  font-weight: bold;
  color: #333;
}

.edit-btn .el-icon {
  font-size: 14px;
  margin: 0;
  padding: 0;
}

.edit-btn {
  max-height: 1px;
  margin: 0;
  padding: 0;
  margin-left: 4px;
}

:deep() .md-editor-preview code {
  font-family: source-code-pro, Menlo, Monaco, Consolas, Courier New, monospace !important;
}

:deep() .cm-editor,
.cm-scroller,
.cm-content,
.cm-content *,
.cm-line {
  font-size: 20px !important;
  line-height: 24px !important;
}

:deep() .md-editor .ͼ1 .cm-scroller,
.md-editor .cm-scroller .cm-line {
  font-size: 20px !important;
  line-height: 24px !important;
}

:deep() .md-editor-custom-scrollbar__track {
  width: 12px !important;
}

:deep() .md-editor-custom-scrollbar__thumb {
  width: 12px !important;
  border-radius: 0 !important;
}

:deep() .md-editor-code-head {
  z-index: 0 !important;
}
</style>