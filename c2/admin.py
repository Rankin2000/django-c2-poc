from django.contrib import admin
from .models import Implant, Task

# Register your models here.
@admin.register(Implant)
class ImplantAdmin(admin.ModelAdmin):
    list_display = ('implant_id', 'last_seen', 'active')
    search_fields = ('implant_id',)

@admin.register(Task)
class TaskAdmin(admin.ModelAdmin):
    list_display = ('implant', 'command', 'completed', 'created_at')
    search_fields = ('implant__implant_id', 'command')
    list_filter = ('completed',)

