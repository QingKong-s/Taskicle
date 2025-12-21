<template>
  <div class="task-changelog">
    <div v-if="loading" class="empty">加载中…</div>
    <div v-else>
      <div v-if="groups.length === 0" class="empty">暂无变更记录</div>
          <div v-for="(group, gidx) in groups" :key="(group.time !== null ? String(group.time) : '') + '_' + gidx" class="group">
            <div class="time">{{ formatTime(group.time) }}</div>
            <ul class="changes">
              <li v-for="(rec, idx) in group.records" :key="(rec.id || idx)" class="change">
                <span>将<strong>{{ humanName(rec.field_name) }}</strong>从</span>
                <span class="old">{{ renderVal(rec.field_name, rec.old_value) }}</span>
                <span>改为</span>
                <span class="new">{{ renderVal(rec.field_name, rec.new_value) }}</span>
              </li>
            </ul>
          </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, defineProps, defineExpose } from 'vue'
import api from '../utils/api'
import { ElMessage } from 'element-plus'
import getStatus, { showErrorMessage } from '../utils/utils'

const props = defineProps({
  taskId: { type: [String, Number], required: true },
  count: { type: Number, default: 100 },
  page: { type: Number, default: 0 }
})

const loading = ref(false)
const logs = ref([])

function humanName(field) {
  const map = {
    task_name: '名称',
    name: '名称',
    status: '状态',
    state: '状态',
    priority: '优先级',
    description: '描述',
    project_id: '项目',
  }
  return map[field] || field
}

function renderVal(field, val) {
  if (field === 'status' || field === 'state') {
    const n = Number(val)
    const s = getStatus('state', isNaN(n) ? val : n)
    return s?.text ?? String(val)
  }
  if (field === 'priority') {
    const n = Number(val)
    const s = getStatus('priority', isNaN(n) ? val : n)
    return s?.text ?? String(val)
  }
  return val == null ? '' : String(val)
}

function formatTime(ts) {
  const d = new Date(ts * 1000)
  return d.toLocaleString()
}

function groupByTime(items) {
  const groupsMap = new Map()
  items.forEach((it) => {
    const t = it.change_at
    if (!groupsMap.has(t))
      groupsMap.set(t, { time: t, records: [] })
    groupsMap.get(t).records.push(it)
  })

  const arr = Array.from(groupsMap.values())
  arr.sort((a, b) => {
    return b.time - a.time
  })
  return arr
}

async function load() {
  if (!props.taskId) return
  loading.value = true
  try {
    const res = await api.get('/api/task_log',
      { params: { task_id: props.taskId, count: props.count, page: props.page } })
    const j = res.data
    if (j && j.r === 0) {
      logs.value = j.data || []
    } else {
      logs.value = []
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    logs.value = []
    showErrorMessage(ElMessage, e)
  } finally {
    loading.value = false
  }
}

onMounted(() => load())
watch(() => props.taskId, () => load())

const groups = ref([])
watch(logs, (v) => {
  groups.value = groupByTime(v)
}, { immediate: true })

defineExpose({ reload: load })
</script>

<style scoped>
.task-changelog {
  padding: 12px;
  box-sizing: border-box;
  max-width: 100%;
  overflow: hidden;
}
.empty {
  color: #888;
  padding: 12px;
}
.group {
  margin-bottom: 12px;
}
.time {
  font-size: 13px;
  color: #666;
  margin-bottom: 6px;
}
.changes {
  list-style: none;
  padding-left: 0;
  margin: 0;
}
.change {
  padding: 6px 8px;
  border-radius: 6px;
  background: #fafafa;
  margin-bottom: 6px;
  word-break: break-word;
}
.old {
  color: #c9302c;
  text-decoration: line-through;
  margin: 0 6px;
}
.new {
  color: #2b8a3e;
  margin-left: 6px;
}
</style>
