<template>
  <el-dialog title="页面版本历史" v-model="visible" width="900px" :close-on-click-modal="false" append-to-body>
    <div v-if="!pageId" class="empty">未选择页面</div>
    <div v-else class="history-container">

      <div class="toolbar">
        <el-button type="warning" plain :disabled="selectedRows.length !== 2" @click="compareVersions">
          对比选中版本 ({{ selectedRows.length }}/2)
        </el-button>
        <span class="tip">
          <template v-if="selectedRows.length === 0">请选择<b>基准版本</b> (旧)</template>
          <template v-else-if="selectedRows.length === 1">请选择<b>对比版本</b> (新)</template>
          <template v-else>已选定顺序：{{ selectedRows[0].ver_id }} → {{ selectedRows[1].ver_id }}</template>
        </span>
        <el-button icon="Refresh" circle class="refresh-btn" @click="loadVersions" />
      </div>

      <el-table 
        v-loading="loading" 
        :data="versions" 
        style="width:100%" 
        height="400"
        row-key="ver_id"
        @row-click="toggleRowSelection"
        :row-class-name="getRowClass"
      >
        <el-table-column width="55" label="选择">
          <template #default="{ row }">
            <div class="selection-cell">
              <el-checkbox :model-value="isRowSelected(row)" @click.stop="toggleRowSelection(row)" />
              <span v-if="getSelectionOrder(row)" class="order-badge">{{ getSelectionOrder(row) }}</span>
            </div>
          </template>
        </el-table-column>
        
        <el-table-column prop="ver_id" label="版本ID" width="100" sortable />
        <el-table-column prop="create_at" label="提交时间" width="180">
          <template #default="{ row }">{{ formatTime(row.create_at) }}</template>
        </el-table-column>
        <el-table-column prop="description" label="备注/描述" show-overflow-tooltip />
        <el-table-column label="操作" width="160" fixed="right">
          <template #default="{ row }">
            <el-button link type="primary" size="small" @click.stop="viewVersion(row)">查看</el-button>
            <el-popconfirm title="确定要恢复吗？" @confirm="restoreVersion(row)">
              <template #reference>
                <el-button link type="danger" size="small" @click.stop>恢复</el-button>
              </template>
            </el-popconfirm>
          </template>
        </el-table-column>
      </el-table>

      <el-dialog v-model="viewerVisible" :title="viewerTitle" width="800px" append-to-body class="viewer-dialog">
        <div v-loading="contentLoading" class="viewer-body">
          <div v-if="mode === 'view'" class="code-view">
            <pre>{{ viewingContent }}</pre>
          </div>
          <div v-else-if="mode === 'diff'" class="diff-view">
            <div class="diff-header">
              <div class="diff-col">
                <el-tag type="info">旧版本 (基准: {{ diffData.oldVer }})</el-tag>
              </div>
              <div class="diff-col">
                <el-tag type="success">新版本 (对比: {{ diffData.newVer }})</el-tag>
              </div>
            </div>
            <div class="diff-content">
              <div v-for="(part, index) in diffData.parts" :key="index"
                :class="['diff-part', part.added ? 'is-added' : '', part.removed ? 'is-removed' : '']">
                <span>{{ part.value }}</span>
              </div>
            </div>
          </div>
        </div>
        <template #footer>
          <el-button @click="viewerVisible = false">关闭</el-button>
          <el-button v-if="mode === 'view' && currentViewRow" type="primary" @click="restoreVersion(currentViewRow)">
            恢复此版本
          </el-button>
        </template>
      </el-dialog>
    </div>
  </el-dialog>
</template>

<script setup>
import { ref, defineProps, defineExpose, defineEmits } from 'vue'
import api from '@/utils/api'
import { ElMessage } from 'element-plus'
import pako from 'pako'
import * as Diff from 'diff'

const props = defineProps({
  pageId: { type: [String, Number], default: null }
})

const emit = defineEmits(['restore'])

const visible = ref(false)
const loading = ref(false)
const versions = ref([])

const selectedRows = ref([])

const viewerVisible = ref(false)
const contentLoading = ref(false)
const mode = ref('view')
const viewerTitle = ref('')
const viewingContent = ref('')
const currentViewRow = ref(null)

const diffData = ref({ oldVer: '', newVer: '', parts: [] })

function isRowSelected(row) {
  return selectedRows.value.some(r => r.ver_id === row.ver_id)
}

function getSelectionOrder(row) {
  const index = selectedRows.value.findIndex(r => r.ver_id === row.ver_id)
  return index !== -1 ? index + 1 : null
}

function toggleRowSelection(row) {
  const index = selectedRows.value.findIndex(r => r.ver_id === row.ver_id)
  
  if (index !== -1) {
    selectedRows.value.splice(index, 1)
  } else {
    if (selectedRows.value.length >= 2) {
      selectedRows.value.shift()
    }
    selectedRows.value.push(row)
  }
}

function getRowClass({ row }) {
  return isRowSelected(row) ? 'selected-row' : ''
}

function open() {
  visible.value = true
  selectedRows.value = []
  loadVersions()
}

async function loadVersions() {
  if (!props.pageId) return
  loading.value = true
  try {
    const res = await api.get('/api/page_version_list', { params: { page_id: props.pageId, count: 100, page: 0 } })
    if (res.data?.r === 0) {
      versions.value = res.data.data || []
    } else {
      ElMessage.error('获取版本列表失败')
    }
  } catch (e) {
    ElMessage.error('网络错误')
  } finally {
    loading.value = false
  }
}

async function fetchVersionContent(verId) {
  const res = await api.get('/api/page_version_content', {
    params: { page_id: props.pageId, ver_id: verId },
    responseType: 'arraybuffer'
  })
  const parsed = parsePageBuffer(res.data)
  return parsed.content || ''
}

async function compareVersions() {
  if (selectedRows.value.length !== 2) return

  const oldVer = selectedRows.value[0]
  const newVer = selectedRows.value[1]

  mode.value = 'diff'
  viewerTitle.value = `对比：从版本 ${oldVer.ver_id} 到 ${newVer.ver_id}`
  viewerVisible.value = true
  contentLoading.value = true

  diffData.value = { oldVer: oldVer.ver_id, newVer: newVer.ver_id, parts: [] }

  try {
    const [oldContent, newContent] = await Promise.all([
      fetchVersionContent(oldVer.ver_id),
      fetchVersionContent(newVer.ver_id)
    ])
    diffData.value.parts = Diff.diffLines(oldContent, newContent)
  } catch (e) {
    ElMessage.error('加载内容失败')
  } finally {
    contentLoading.value = false
  }
}

async function viewVersion(row) {
  mode.value = 'view'
  viewerTitle.value = `查看版本: ${row.ver_id}`
  currentViewRow.value = row
  viewerVisible.value = true
  contentLoading.value = true
  try {
    viewingContent.value = await fetchVersionContent(row.ver_id)
  } catch (e) {
    ElMessage.error('加载失败')
  } finally {
    contentLoading.value = false
  }
}

async function restoreVersion(row) {
  try {
    const content = await fetchVersionContent(row.ver_id)
    emit('restore', content)
    ElMessage.success(`已恢复版本 ${row.ver_id}`)
    visible.value = false
  } catch (e) {
    ElMessage.error('恢复失败')
  }
}

function parsePageBuffer(buf) {
  if (!buf || buf.byteLength < 36) return {}
  const dv = new DataView(buf)
  const cbContent = dv.getUint32(24, true)
  const bCompressed = dv.getInt32(12, true)
  const contentBytes = new Uint8Array(buf, 36, cbContent)
  let content = ''
  try {
    if (bCompressed) {
      content = new TextDecoder().decode(pako.ungzip(contentBytes))
    } else {
      content = new TextDecoder().decode(contentBytes)
    }
  } catch (e) { content = '[Decode Error]' }
  return { content }
}

function formatTime(ts) {
  return ts ? new Date(ts * 1000).toLocaleString() : ''
}

defineExpose({ open })
</script>

<style scoped>
.empty {
  color: #888;
  padding: 20px;
  text-align: center;
}

.history-container {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.toolbar {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 0 5px;
}

.refresh-btn {
  margin-left: auto;
}

.tip {
  color: #909399;
  font-size: 12px;
  margin-left: 10px;
}

.viewer-body {
  min-height: 300px;
  max-height: 60vh;
  overflow-y: auto;
}

.code-view pre {
  background: #f5f7fa;
  padding: 15px;
  border-radius: 4px;
  margin: 0;
  white-space: pre-wrap;
  word-break: break-all;
  font-family: Consolas, Monaco, monospace;
}

.diff-view {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.diff-header {
  display: flex;
  justify-content: space-between;
  padding-bottom: 10px;
  border-bottom: 1px solid #eee;
}

.diff-content {
  font-family: Consolas, Monaco, monospace;
  background: #fff;
  font-size: 13px;
  line-height: 1.5;
}

.diff-part {
  white-space: pre-wrap;
  word-break: break-all;
  padding: 2px 4px;
}

.is-added {
  background-color: #e6ffec;
  color: #155724;
}

.is-removed {
  background-color: #ffebe9;
  color: #cb2431;
  text-decoration: line-through;
  opacity: 0.8;
}
</style>