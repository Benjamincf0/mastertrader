# Look up Canonical's current Ubuntu 24.04 image.
data "aws_ssm_parameter" "ubuntu" {
  name = "/aws/service/canonical/ubuntu/server/24.04/stable/current/amd64/hvm/ebs-gp3/ami-id"
}

# Upload only the public half of your SSH key.
resource "aws_key_pair" "setup" {
  key_name   = "mastertrader-setup"
  public_key = file(pathexpand("~/.ssh/mastertrader.pub"))
}

resource "aws_security_group" "setup" {
  name        = "mastertrader-setup"
  description = "SSH access for the Phase 0 exercise"
  vpc_id      = "vpc-02df3c7414b540332"
}

resource "aws_vpc_security_group_ingress_rule" "ssh" {
  security_group_id = aws_security_group.setup.id
  cidr_ipv4         = "38.34.47.164/32"
  ip_protocol       = "tcp"
  from_port         = 22
  to_port           = 22
}

resource "aws_vpc_security_group_egress_rule" "outbound" {
  security_group_id = aws_security_group.setup.id
  cidr_ipv4         = "0.0.0.0/0"
  ip_protocol       = "-1"
}

resource "aws_instance" "setup" {
  ami                         = nonsensitive(data.aws_ssm_parameter.ubuntu.value)
  instance_type               = "t3.micro"
  subnet_id                   = "subnet-0ab24cc3fa727cd92"
  associate_public_ip_address = true
  key_name                    = aws_key_pair.setup.key_name
  vpc_security_group_ids      = [aws_security_group.setup.id]

  root_block_device {
    volume_size           = 8
    volume_type           = "gp3"
    encrypted             = true
    delete_on_termination = true
  }

  metadata_options {
    http_tokens = "required"
  }

  credit_specification {
    cpu_credits = "standard"
  }

  tags = {
    Name = "mastertrader-phase0"
  }
}

output "instance_public_ip" {
  value = aws_instance.setup.public_ip
}
