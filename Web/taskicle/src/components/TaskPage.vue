<template>
  <div class="task-page">
    <div class="left-panel" :style="{ width: leftWidth + 'px' }">
      <div class="left-toolbar">
        <el-button class="new-task-btn" type="primary" @click="onCreateTask">
          + 新建任务
        </el-button>
      </div>
      <el-table class="left" :show-header="false" :data="tableData" @row-click="onRowClick" style="width: 100%"
        :row-style="getColor">
        <el-table-column>
          <template #default="scope">
            <div class="row">
              <span class="capsule id-capsule">#{{ scope.row.task_id }}</span>

              <span class="capsule priority-capsule" :style="{
                background: getStatus('priority', scope.row.priority).background,
                color: getStatus('priority', scope.row.priority).color
              }">
                {{ getStatus('priority', scope.row.priority).text }}
              </span>

              <span class="task-name" :title="scope.row.name">{{ scope.row.task_name }}</span>

              <span class="capsule state-capsule" :style="{
                background: getStatus('state', scope.row.status).background,
                color: getStatus('state', scope.row.status).color
              }">
                {{ getStatus('state', scope.row.status).text }}
              </span>
            </div>
          </template>
        </el-table-column>
      </el-table>
    </div>
    <Splitter @ondrag="onDrag" />
    <TaskDetail class="right" :task="selectedTask" @updated="onTaskUpdated" @deleted="onDeleteTask"
      @created="onTaskCreated" />
  </div>
</template>

<script setup>
import TaskDetail from './TaskDetail.vue';
import Splitter from './SplitBar.vue';
import { ref, onMounted, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router'
import { ElMessageBox, ElMessage } from 'element-plus'
import api from '../utils/api'
import { getStatus, showErrorMessage } from '../utils/utils'

const leftWidth = ref(0);
const defaultDetailWidth = 520;

function computeDefaultWidth() {
  const page = document.querySelector('.task-page');
  if (page) {
    return Math.max(200, page.clientWidth - defaultDetailWidth);
  }
  return 800;
}

const route = useRoute()
const router = useRouter()
const selectedTask = ref(null)

onMounted(() => {
  leftWidth.value = computeDefaultWidth();
  fetchTasksForProject(parseInt(route.query.project))
});

watch(() => route.query.project, (v) => {
  fetchTasksForProject(parseInt(v))
})

watch(() => route.query.task, (t) => {
  if (t && tableData.value.length) {
    const task = tableData.value.find(item => String(item.task_id) === String(t))
    if (task) {
      selectedTask.value = task
    }
  }
})

function onDrag(movementX) {
  leftWidth.value = Math.max(200, leftWidth.value + movementX);
}

async function fetchTasksForProject(projId) {
  if (!projId) {
    tableData.value = []
    selectedTask.value = null
    return
  }
  try {
    const res = await api.get('/api/task_list',
      { params: { project_id: projId, count: 100, page: 0 } })
    const j = res.data
    if (j && j.r === 0) {
      const items = (j.data || []).map(it => ({ ...it, project_id: projId }))
      tableData.value = items
      if (items.length) {
        const taskId = route.query.task
        if (taskId) {
          const task = items.find(item => String(item.task_id) === String(taskId))
          selectedTask.value = task || items[0]
        } else {
          selectedTask.value = items[0]
        }
      } else {
        selectedTask.value = null
      }
    } else {
      tableData.value = []
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    tableData.value = []
    showErrorMessage(ElMessage, e)
  }
}

async function onCreateTask() {
  const projId = parseInt(route.query.project)
  if (!projId) {
    ElMessage.warning('请先选择一个项目')
    return
  }
  const newTask = {
    project_id: projId,
    task_name: '新任务',
    description: '',
    priority: 2,
    status: 0,
    isNew: true,
  }
  selectedTask.value = newTask
}

async function onDeleteTask(row) {
  try {
    await ElMessageBox.confirm(`确认删除任务：${row.task_name || row.name || row.task_id || row.id}？`, '删除任务', {
      confirmButtonText: '删除',
      cancelButtonText: '取消',
      type: 'warning',
    })
    const res = await api.post('/api/task_delete', { task_id: row.task_id || row.id })
    const j = res.data
    if (j && j.r === 0) {
      ElMessage.success('删除成功')
      await fetchTasksForProject(parseInt(route.query.project))
    } else {
      showErrorMessage(ElMessage, j)
    }
  } catch (e) {
    showErrorMessage(ElMessage, e)
  }
}

function onRowClick(row) {
  selectedTask.value = row
  router.push({ name: 'tasks', query: { project: route.query.project, task: row.task_id } })
}

function isSelected(row) {
  if (!selectedTask.value) return false
  const rid = row.task_id || row.id
  const sid = selectedTask.value.task_id || selectedTask.value.id
  return rid === sid
}

function getColor({ row }) {
  if (isSelected(row)) {
    return "background: #e6f4ff"
  }
  return ""
}

function onTaskUpdated(updated) {
  if (!updated) return
  const id = updated.task_id || updated.id
  const idx = tableData.value.findIndex(it => (it.task_id || it.id) === id)
  if (idx !== -1) {
    tableData.value[idx] = Object.assign({}, tableData.value[idx], updated)
  }
  if (selectedTask.value && (selectedTask.value.task_id || selectedTask.value.id) === id) {
    selectedTask.value = Object.assign({}, selectedTask.value, updated)
  }
}

async function onTaskCreated(created) {
  const projId = parseInt(route.query.project)
  if (!projId) return
  await fetchTasksForProject(projId)
  const id = created.task_id || created.id
  const found = tableData.value.find(it => (it.task_id || it.id) === id)
  selectedTask.value = found || null
}

const tableData = ref([
]);
</script>

<style scoped>
.task-page {
  display: flex;
  height: 100%;
  overflow: hidden;
}

.left {
  border-right: 1px solid #ebeef5;
  height: 100%;
  overflow-y: auto;
}

.left-panel {
  border-right: 1px solid #ebeef5;
  height: 100%;
  display: flex;
  flex-direction: column;
}

.left-toolbar {
  padding: 8px;
  border-bottom: 1px solid #f5f7fa;
}

.left-toolbar .new-task-btn {
  height: 40px;
  padding: 6px 16px;
  font-size: 14px;
}

.right {
  flex: 1;
  padding: 16px;
  overflow-y: auto;
  min-height: 0;
}

.row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 1px 0;
  font-size: 14px;
}

.is-selected {
  background-color: azure;
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

.state-capsule {
  margin-left: auto;
}

/* 最低 */
.p-0 {
  background: #f2f3f5;
  color: #7b7b7b;
}

/* 较低 */
.p-1 {
  background: #e6f7ed;
  color: #3f8c45;
}

/* 正常 */
.p-2 {
  background: #e6f3ff;
  color: #1677ff;
}

/* 较高 */
.p-3 {
  background: #fff4e5;
  color: #d48806;
}

/* 最高 */
.p-4 {
  background: #ffe8e8;
  color: #cf1322;
}

/* 未开始 */
.s-0 {
  background: #f0f0f0;
  color: #666;
}

/* 进行中 */
.s-1 {
  background: #e6f4ff;
  color: #095cb5;
}

/* 已完成 */
.s-2 {
  background: #e7f9ef;
  color: #2a8a43;
}

/* 关闭 */
.s-3 {
  background: #f8e6e6;
  color: #b32626;
}

/* 挂起 */
.s-4 {
  background: #fff5d6;
  color: #ad7a00;
}

/* 待验证 */
.s-5 {
  background: #f1e6ff;
  color: #6a32c9;
}
</style>