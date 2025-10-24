from django.db import models

# Create your models here.
class Implant(models.Model):
    implant_id = models.CharField(max_length=100, unique=True)
    last_seen = models.DateTimeField(auto_now=True)
    active = models.BooleanField(default=True)

    def __str__(self):
        return self.implant_id

class Task(models.Model):
    implant = models.ForeignKey(Implant, on_delete=models.CASCADE, related_name='tasks')
    command = models.TextField()
    created_at = models.DateTimeField(auto_now_add=True)
    completed = models.BooleanField(default=False)
    results = models.TextField(blank=True, null=True)

    def __str__(self):
        return f"Task for {self.implant.implant_id}: {self.command[:20]}..."
    
