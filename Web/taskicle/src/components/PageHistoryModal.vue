<template>
  <el-dialog title="页面历史" v-model="visible" width="700px">
    <div v-if="!pageId" class="empty">未选择页面</div>
    <div v-else>
      <div v-if="loading" class="empty">加载中…</div>
      <div v-else>
        <el-table :data="versions" style="width:100%" v-if="versions.length">
          <el-table-column prop="ver_id" label="版本ID" width="120"/>
          <el-table-column prop="create_at" label="时间" width="180">
            <template #default="{ row }">{{ formatTime(row.create_at) }}</template>
          </el-table-column>
          <el-table-column prop="description" label="描述"/>
          <el-table-column label="操作" width="120">
            <template #default="{ row }">
              <el-button size="mini" type="primary" @click="viewVersion(row.ver_id)">查看</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div v-if="versions.length === 0" class="empty">暂无历史版本</div>
        <div v-if="viewing" class="viewer">
          <div class="viewer-header">
            <strong>版本 {{ viewing.ver_id }}</strong>
            <el-button size="mini" @click="closeViewer">关闭</el-button>
          </div>
          <pre class="viewer-content">{{ viewing.content }}</pre>
        </div>
      </div>
    </div>
  </el-dialog>
</template>

<script setup>
import { ref, watch, defineProps, defineExpose } from 'vue'
import api from '@/utils/api'
import { ElMessage } from 'element-plus'
import { showAxiosErrorMessage, showErrorMessage } from '@/utils/utils'
import pako from 'pako'

const props = defineProps({
  pageId: { type: [String, Number], default: null }
})

const visible = ref(false)
const loading = ref(false)
const versions = ref([])
const viewing = ref(null)

function open() { 
  visible.value = true
  loadVersions()
}
function close() { 
  visible.value = false }

async function loadVersions() {
  if (!props.pageId) return
  loading.value = true
  try {
    const res = await api.get('/api/page_version_list', { params: { page_id: props.pageId, count: 100, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      versions.value = j.data || []
    } else {
      versions.value = []
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    versions.value = []
    showAxiosErrorMessage(ElMessage, e)
  } finally {
    loading.value = false
  }
}

watch(() => props.pageId, () => {
  if (visible.value) loadVersions()
})

async function viewVersion(verId) {
  if (!props.pageId) return
  try {
    const res = await api.get('/api/page_version_content', { params: { page_id: props.pageId, ver_id: verId }, responseType: 'arraybuffer' })
    const parsed = parsePageBuffer(res.data)
    viewing.value = { ver_id: verId, content: parsed.content || '' }
  } catch (e) {
    showAxiosErrorMessage(ElMessage, e)
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

function formatTime(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return d.toLocaleString()
}

function closeViewer() { viewing.value = null }

defineExpose({ open, close })
</script>

<style scoped>
.empty { color: #888; padding: 12px }
.viewer { margin-top: 12px }
.viewer-header { display:flex; justify-content:space-between; align-items:center }
.viewer-content { white-space:pre-wrap; background:#fafafa; padding:12px; border-radius:6px; max-height:320px; overflow:auto }
</style>
